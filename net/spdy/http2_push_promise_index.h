// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HTTP2_PUSH_PROMISE_INDEX_H_
#define NET_SPDY_HTTP2_PUSH_PROMISE_INDEX_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/http/http_request_info.h"
#include "net/spdy/spdy_session_key.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"
#include "url/gurl.h"

namespace net {

class SpdySession;

namespace test {

class Http2PushPromiseIndexPeer;

}  // namespace test

// Value returned by ClaimPushedStream() and FindStream() if no stream is found.
const spdy::SpdyStreamId kNoPushedStreamFound = 0;

// This class manages unclaimed pushed streams (push promises) from the receipt
// of PUSH_PROMISE frame until they are matched to a request.
// Each SpdySessionPool owns one instance of this class.
// SpdySession uses this class to register, unregister and query pushed streams.
// HttpStreamFactory::Job uses this class to find a SpdySession with a pushed
// stream matching the request, if such exists.
class NET_EXPORT Http2PushPromiseIndex {
 public:
  // Interface for validating pushed streams, signaling when a pushed stream is
  // claimed, and generating SpdySession weak pointer.
  class NET_EXPORT Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Return true if a pushed stream with |url| can be used for a request with
    // |key|.
    virtual bool ValidatePushedStream(spdy::SpdyStreamId stream_id,
                                      const GURL& url,
                                      const HttpRequestInfo& request_info,
                                      const SpdySessionKey& key) const = 0;

    // Generate weak pointer.  Guaranateed to be called synchronously after
    // ValidatePushedStream() is called and returns true.
    virtual base::WeakPtr<SpdySession> GetWeakPtrToSession() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  Http2PushPromiseIndex();
  ~Http2PushPromiseIndex();

  // Tries to register a Delegate with an unclaimed pushed stream for |url|.
  // Caller must make sure |delegate| stays valid by unregistering the exact
  // same entry before |delegate| is destroyed.
  // Returns true if there is no unclaimed pushed stream with the same URL for
  // the same Delegate, in which case the stream is registered.
  bool RegisterUnclaimedPushedStream(const GURL& url,
                                     spdy::SpdyStreamId stream_id,
                                     Delegate* delegate) WARN_UNUSED_RESULT;

  // Tries to unregister a Delegate with an unclaimed pushed stream for |url|
  // with given |stream_id|.
  // Returns true if this exact entry is found, in which case it is removed.
  bool UnregisterUnclaimedPushedStream(const GURL& url,
                                       spdy::SpdyStreamId stream_id,
                                       Delegate* delegate) WARN_UNUSED_RESULT;

  // Returns the number of pushed streams registered for |delegate|.
  size_t CountStreamsForSession(const Delegate* delegate) const;

  // Returns the stream ID of the entry registered for |delegate| with |url|,
  // or kNoPushedStreamFound if no such entry exists.
  spdy::SpdyStreamId FindStream(const GURL& url,
                                const Delegate* delegate) const;

  // If there exists a session compatible with |key| that has an unclaimed push
  // stream for |url|, then sets |*session| and |*stream| to one such session
  // and stream, and removes entry from index.  Makes no guarantee on which
  // (session, stream_id) pair is claimed if there are multiple matches.  Sets
  // |*session| to nullptr and |*stream| to kNoPushedStreamFound if no such
  // session exists.
  void ClaimPushedStream(const SpdySessionKey& key,
                         const GURL& url,
                         const HttpRequestInfo& request_info,
                         base::WeakPtr<SpdySession>* session,
                         spdy::SpdyStreamId* stream_id);

  // Return the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  friend test::Http2PushPromiseIndexPeer;

  // An unclaimed pushed stream entry.
  struct NET_EXPORT UnclaimedPushedStream {
    GURL url;
    Delegate* delegate;
    spdy::SpdyStreamId stream_id;
    size_t EstimateMemoryUsage() const;
  };

  // Function object satisfying the requirements of "Compare", see
  // http://en.cppreference.com/w/cpp/concept/Compare.
  // A set ordered by this function object supports O(log n) lookup
  // of the first entry with a given URL, by calling lower_bound() with an entry
  // with that URL and with delegate = nullptr.
  struct NET_EXPORT CompareByUrl {
    bool operator()(const UnclaimedPushedStream& a,
                    const UnclaimedPushedStream& b) const;
  };

  // Collection of all unclaimed pushed streams.  Delegate must unregister
  // its streams before destruction, so that all pointers remain valid.
  // Each Delegate can have at most one pushed stream for each URL (but each
  // Delegate can have pushed streams for different URLs, and different
  // Delegates can have pushed streams for the same GURL).
  std::set<UnclaimedPushedStream, CompareByUrl> unclaimed_pushed_streams_;

  DISALLOW_COPY_AND_ASSIGN(Http2PushPromiseIndex);
};

}  // namespace net

#endif  // NET_SPDY_HTTP2_PUSH_PROMISE_INDEX_H_
