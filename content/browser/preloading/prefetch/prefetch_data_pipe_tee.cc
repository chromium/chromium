// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_data_pipe_tee.h"

#include "base/containers/span.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "services/network/public/cpp/features.h"

namespace content {

namespace {

MojoResult CreateDataPipeForServingData(
    mojo::ScopedDataPipeProducerHandle& producer_handle,
    mojo::ScopedDataPipeConsumerHandle& consumer_handle) {
  MojoCreateDataPipeOptions options;

  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      network::features::GetDataPipeDefaultAllocationSize(
          network::features::DataPipeAllocationSize::kLargerSizeIfPossible);

  return mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
}

}  // namespace

PrefetchDataPipeTee::PrefetchDataPipeTee(
    mojo::ScopedDataPipeConsumerHandle source,
    size_t buffer_limit)
    : source_(std::move(source)),
      source_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      base::SequencedTaskRunner::GetCurrentDefault()),
      buffer_limit_(buffer_limit),
      target_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                      base::SequencedTaskRunner::GetCurrentDefault()) {
  source_watcher_.Watch(source_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                        base::BindRepeating(&PrefetchDataPipeTee::OnReadable,
                                            weak_factory_.GetWeakPtr()));
  source_watcher_.ArmOrNotify();
}

PrefetchDataPipeTee::~PrefetchDataPipeTee() {
  CHECK(!target_.first);
}

mojo::ScopedDataPipeConsumerHandle PrefetchDataPipeTee::Clone() {
  switch (state_) {
    case State::kLoading:
      if (target_.first || pending_writes_) {
        return {};
      }
      break;
    case State::kSizeExceededNoTarget:
      CHECK(!target_.first);
      CHECK_EQ(pending_writes_, 0u);
      state_ = State::kSizeExceeded;
      break;
    case State::kSizeExceeded:
      return {};
    case State::kLoaded:
      break;
  }

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv =
      CreateDataPipeForServingData(producer_handle, consumer_handle);
  if (rv != MOJO_RESULT_OK) {
    return {};
  }

  auto producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));

  // Send `buffer_` (== the whole data read so far) to the new target.
  WriteData(std::make_pair(std::move(producer), base::WrapRefCounted(this)),
            buffer_);

  return consumer_handle;
}

void PrefetchDataPipeTee::OnReadable(MojoResult result,
                                     const mojo::HandleSignalsState& state) {
  if (pending_writes_) {
    // Reading is blocked while writing to a target is ongoing.
    return;
  }

  switch (state_) {
    case State::kLoading:
    case State::kSizeExceeded:
      break;
    case State::kSizeExceededNoTarget:
      // Reading is blocked until the first target is added, because there are
      // no buffer space to store the read data until the target is added.
      return;
    case State::kLoaded:
      // No further read is needed.
      return;
  }

  base::span<const uint8_t> read_data;
  MojoResult rv = source_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, read_data);
  if (rv == MOJO_RESULT_OK) {
    switch (state_) {
      case State::kLoading:
        CHECK_LE(buffer_.size(), buffer_limit_);
        if (buffer_.size() + read_data.size() <= buffer_limit_) {
          buffer_.append(base::as_string_view(read_data));
          if (target_.first) {
            WriteData(ResetTarget({}),  //
                      std::string_view(buffer_).substr(buffer_.size() -
                                                       read_data.size()));
          }
          break;
        }

        // If there are no targets yet, stop reading and keep `buffer_` (== the
        // whole data read so far) until the first target is added.
        if (!target_.first) {
          read_data = read_data.first(buffer_limit_ - buffer_.size());
          buffer_.append(base::as_string_view(read_data));
          state_ = State::kSizeExceededNoTarget;
          break;
        }

        // If there is a target, clear the current `buffer_` because it was
        // already written to the target. The current read data is written to
        // the target below.
        buffer_.clear();
        state_ = State::kSizeExceeded;
        [[fallthrough]];

      case State::kSizeExceeded:
        CHECK(buffer_.empty());
        if (!target_.first) {
          // The target was already removed. Discard the current read data,
          // because anyway no targets can be added to `this`.
          break;
        }

        buffer_.append(base::as_string_view(read_data));
        WriteData(ResetTarget({}), buffer_);
        break;
      case State::kSizeExceededNoTarget:
      case State::kLoaded:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    source_->EndReadData(read_data.size());
    source_watcher_.ArmOrNotify();
  } else if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
    switch (state_) {
      case State::kLoading:
        state_ = State::kLoaded;
        // Closes the producer handle, if any.
        ResetTarget({});
        break;
      case State::kSizeExceeded:
        // Closes the producer handle, if any.
        ResetTarget({});
        break;
      case State::kSizeExceededNoTarget:
      case State::kLoaded:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  } else if (rv != MOJO_RESULT_SHOULD_WAIT) {
    CHECK(false) << "Unhandled MojoResult: " << rv;
  }
}

PrefetchDataPipeTee::ProducerPair PrefetchDataPipeTee::ResetTarget(
    ProducerPair target) {
  auto old_target = std::move(target_);
  target_ = std::move(target);

  target_watcher_.Cancel();
  if (target_.first) {
    CHECK_EQ(target_.second.get(), this);
    // Detect disconnection during `target_` is set and no ongoing writes for
    // `target_` is performed. Disconnection during ongoing writes are detected
    // and handled by `OnDataWritten()`.
    target_watcher_.Watch(
        target_.first->GetProducerHandle(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&PrefetchDataPipeTee::OnWriteDataPipeClosed,
                            weak_factory_.GetWeakPtr()));
  }

  return old_target;
}

void PrefetchDataPipeTee::OnWriteDataPipeClosed(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  CHECK(target_.first);
  if (state.peer_closed()) {
    ResetTarget({});
  }
}

void PrefetchDataPipeTee::WriteData(ProducerPair target,
                                    base::span<const char> data) {
  CHECK_EQ(target.second.get(), this);
  ++pending_writes_;
  auto* raw_target = target.first.get();
  raw_target->Write(std::make_unique<mojo::StringDataSource>(
                        data, mojo::StringDataSource::AsyncWritingMode::
                                  STRING_STAYS_VALID_UNTIL_COMPLETION),
                    base::BindOnce(&PrefetchDataPipeTee::OnDataWritten,
                                   base::Unretained(this), std::move(target)));
}

void PrefetchDataPipeTee::OnDataWritten(ProducerPair target,
                                        MojoResult result) {
  CHECK_GT(pending_writes_, 0u);
  --pending_writes_;

  switch (state_) {
    case State::kLoaded:
      // Destruct `target`, because all data (== `buffer_`) is written to
      // `target`.
      break;
    case State::kSizeExceeded:
      // Data are streamed and thus cleared after written.
      // `buffer_` is kept until here because
      // `STRING_STAYS_VALID_UNTIL_COMPLETION` is used.
      buffer_.clear();
      [[fallthrough]];
    case State::kLoading:
      // Continue writing on `target` by setting it to `target_` again on
      // successful writes, while destruct `target` on errors.
      if (result == MOJO_RESULT_OK) {
        ResetTarget(std::move(target));
      }
      // In either case continue loading because `buffer_` and upcoming data can
      // be still used for future targets.
      if (pending_writes_ == 0) {
        source_watcher_.ArmOrNotify();
      }
      break;
    case State::kSizeExceededNoTarget:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

}  // namespace content
