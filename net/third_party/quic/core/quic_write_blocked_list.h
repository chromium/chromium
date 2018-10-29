// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_WRITE_BLOCKED_LIST_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_WRITE_BLOCKED_LIST_H_

#include <cstddef>
#include <cstdint>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"
#include "net/third_party/spdy/core/priority_write_scheduler.h"

namespace quic {

// Keeps tracks of the QUIC streams that have data to write, sorted by
// priority.  QUIC stream priority order is:
// Crypto stream > Headers stream > Data streams by requested priority.
class QUIC_EXPORT_PRIVATE QuicWriteBlockedList {
 private:
  typedef spdy::PriorityWriteScheduler<QuicStreamId> QuicPriorityWriteScheduler;

 public:
  explicit QuicWriteBlockedList();
  QuicWriteBlockedList(const QuicWriteBlockedList&) = delete;
  QuicWriteBlockedList& operator=(const QuicWriteBlockedList&) = delete;
  ~QuicWriteBlockedList();

  bool HasWriteBlockedDataStreams() const {
    return priority_write_scheduler_.HasReadyStreams();
  }

  bool HasWriteBlockedSpecialStream() const {
    return static_stream_collection_.num_blocked() > 0;
  }

  size_t NumBlockedSpecialStreams() const {
    return static_stream_collection_.num_blocked();
  }

  size_t NumBlockedStreams() const {
    return NumBlockedSpecialStreams() +
           priority_write_scheduler_.NumReadyStreams();
  }

  bool ShouldYield(QuicStreamId id) const {
    for (const auto& stream : static_stream_collection_) {
      if (stream.id == id) {
        // Static streams should never yield to data streams, or to lower
        // priority static stream.
        return false;
      }
      if (stream.is_blocked) {
        return true;  // All data streams yield to static streams.
      }
    }

    return priority_write_scheduler_.ShouldYield(id);
  }

  // Pops the highest priorty stream, special casing crypto and headers streams.
  // Latches the most recently popped data stream for batch writing purposes.
  QuicStreamId PopFront() {
    QuicStreamId static_stream_id;
    if (static_stream_collection_.UnblockFirstBlocked(&static_stream_id)) {
      return static_stream_id;
    }

    const auto id_and_precedence =
        priority_write_scheduler_.PopNextReadyStreamAndPrecedence();
    const QuicStreamId id = std::get<0>(id_and_precedence);
    const spdy::SpdyPriority priority =
        std::get<1>(id_and_precedence).spdy3_priority();

    if (!priority_write_scheduler_.HasReadyStreams()) {
      // If no streams are blocked, don't bother latching.  This stream will be
      // the first popped for its priority anyway.
      batch_write_stream_id_[priority] = 0;
      last_priority_popped_ = priority;
    } else if (batch_write_stream_id_[priority] != id) {
      // If newly latching this batch write stream, let it write 16k.
      batch_write_stream_id_[priority] = id;
      bytes_left_for_batch_write_[priority] = 16000;
      last_priority_popped_ = priority;
    }

    return id;
  }

  void RegisterStream(QuicStreamId stream_id,
                      bool is_static_stream,
                      spdy::SpdyPriority priority) {
    DCHECK(!priority_write_scheduler_.StreamRegistered(stream_id));
    if (is_static_stream) {
      static_stream_collection_.Register(stream_id);
      return;
    }

    priority_write_scheduler_.RegisterStream(
        stream_id, spdy::SpdyStreamPrecedence(priority));
  }

  void UnregisterStream(QuicStreamId stream_id, bool is_static) {
    if (is_static) {
      static_stream_collection_.Unregister(stream_id);
      return;
    }
    priority_write_scheduler_.UnregisterStream(stream_id);
  }

  void UpdateStreamPriority(QuicStreamId stream_id,
                            spdy::SpdyPriority new_priority) {
    DCHECK(!static_stream_collection_.IsRegistered(stream_id));
    priority_write_scheduler_.UpdateStreamPrecedence(
        stream_id, spdy::SpdyStreamPrecedence(new_priority));
  }

  void UpdateBytesForStream(QuicStreamId stream_id, size_t bytes) {
    if (batch_write_stream_id_[last_priority_popped_] == stream_id) {
      // If this was the last data stream popped by PopFront, update the
      // bytes remaining in its batch write.
      bytes_left_for_batch_write_[last_priority_popped_] -=
          static_cast<int32_t>(bytes);
    }
  }

  // Pushes a stream to the back of the list for its priority level *unless* it
  // is latched for doing batched writes in which case it goes to the front of
  // the list for its priority level.
  // Headers and crypto streams are special cased to always resume first.
  void AddStream(QuicStreamId stream_id) {
    if (static_stream_collection_.SetBlocked(stream_id)) {
      return;
    }

    bool push_front =
        stream_id == batch_write_stream_id_[last_priority_popped_] &&
        bytes_left_for_batch_write_[last_priority_popped_] > 0;
    priority_write_scheduler_.MarkStreamReady(stream_id, push_front);
  }

  // Returns true if stream with |stream_id| is write blocked.
  bool IsStreamBlocked(QuicStreamId stream_id) const {
    for (const auto& stream : static_stream_collection_) {
      if (stream.id == stream_id) {
        return stream.is_blocked;
      }
    }

    return priority_write_scheduler_.IsStreamReady(stream_id);
  }

 private:
  QuicPriorityWriteScheduler priority_write_scheduler_;

  // If performing batch writes, this will be the stream ID of the stream doing
  // batch writes for this priority level.  We will allow this stream to write
  // until it has written kBatchWriteSize bytes, it has no more data to write,
  // or a higher priority stream preempts.
  QuicStreamId batch_write_stream_id_[spdy::kV3LowestPriority + 1];
  // Set to kBatchWriteSize when we set a new batch_write_stream_id_ for a given
  // priority.  This is decremented with each write the stream does until it is
  // done with its batch write.
  int32_t bytes_left_for_batch_write_[spdy::kV3LowestPriority + 1];
  // Tracks the last priority popped for UpdateBytesForStream.
  spdy::SpdyPriority last_priority_popped_;

  // A StaticStreamCollection is a vector of <QuicStreamId, bool> pairs plus a
  // eagerly-computed number of blocked static streams.
  class StaticStreamCollection {
   public:
    struct StreamIdBlockedPair {
      QuicStreamId id;
      bool is_blocked;
    };

    // Optimized for the typical case of 2 static streams per session.
    typedef QuicInlinedVector<StreamIdBlockedPair, 2> StreamsVector;

    StreamsVector::const_iterator begin() const { return streams_.cbegin(); }

    StreamsVector::const_iterator end() const { return streams_.cend(); }

    size_t num_blocked() const { return num_blocked_; }

    // Add |id| to the collection in unblocked state.
    void Register(QuicStreamId id) {
      DCHECK(!IsRegistered(id));
      DCHECK(streams_.empty() || id > streams_.back().id)
          << "stream_id: " << id
          << " last static stream: " << streams_.back().id;
      streams_.push_back({id, false});
    }

    // True if |id| is in the collection, regardless of its state.
    bool IsRegistered(QuicStreamId id) const {
      for (const auto& stream : streams_) {
        if (stream.id == id) {
          return true;
        }
      }
      return false;
    }

    // Remove |id| from the collection, if it is in the blocked state, reduce
    // |num_blocked_| by 1.
    void Unregister(QuicStreamId id) {
      for (auto it = streams_.begin(); it != streams_.end(); ++it) {
        if (it->id == id) {
          if (it->is_blocked) {
            --num_blocked_;
          }
          streams_.erase(it);
          return;
        }
      }
      DCHECK(false) << "Erasing a non-exist stream with id " << id;
    }

    // Set |id| to be blocked. If |id| is not already blocked, increase
    // |num_blocked_| by 1.
    // Return true if |id| is in the collection.
    bool SetBlocked(QuicStreamId id) {
      for (auto& stream : streams_) {
        if (stream.id == id) {
          if (!stream.is_blocked) {
            stream.is_blocked = true;
            ++num_blocked_;
          }
          return true;
        }
      }
      return false;
    }

    // Unblock the first blocked stream in the collection.
    // If no stream is blocked, return false. Otherwise return true, set *id to
    // the unblocked stream id and reduce |num_blocked_| by 1.
    bool UnblockFirstBlocked(QuicStreamId* id) {
      for (auto& stream : streams_) {
        if (stream.is_blocked) {
          --num_blocked_;
          stream.is_blocked = false;
          *id = stream.id;
          return true;
        }
      }
      return false;
    }

   private:
    size_t num_blocked_ = 0;
    StreamsVector streams_;
  };

  StaticStreamCollection static_stream_collection_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_WRITE_BLOCKED_LIST_H_
