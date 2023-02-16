// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/hls_data_source_provider_impl.h"

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/types/pass_key.h"
#include "media/formats/hls/types.h"
#include "third_party/blink/renderer/platform/media/buffered_data_source_host_impl.h"
#include "third_party/blink/renderer/platform/media/multi_buffer_data_source.h"

namespace blink {

class HlsDataSourceImpl final : public media::HlsDataSource {
 public:
  HlsDataSourceImpl(
      base::WeakPtr<HlsDataSourceProviderImpl> provider,
      std::unique_ptr<MultiBufferDataSource> mb_data_source,
      absl::optional<media::hls::types::ByteRange> range,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner)
      : media::HlsDataSource(DetermineSize(*mb_data_source, range)),
        provider_(std::move(provider)),
        mb_data_source_(std::move(mb_data_source)),
        range_(range),
        main_task_runner_(std::move(main_task_runner)),
        media_task_runner_(std::move(media_task_runner)) {
    DCHECK(mb_data_source_);
    DCHECK(main_task_runner_);
    DCHECK(media_task_runner_);
    DCHECK(main_task_runner_->BelongsToCurrentThread());
  }
  ~HlsDataSourceImpl() override {
    DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

    // Notify the provider that we've been destroyed so it can remove the
    // `MultiBufferDataSource` from its active set.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HlsDataSourceProviderImpl::NotifyDataSourceDestroyed,
                       std::move(provider_), base::PassKey<HlsDataSourceImpl>(),
                       std::move(mb_data_source_)));
  }

  void Read(uint64_t pos,
            size_t size,
            uint8_t* buffer,
            ReadCb callback) override {
    DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

    // Read into the byterange, if one was given.
    // Caller will be using 0-based positions regardless.
    if (range_.has_value()) {
      pos += range_->GetOffset();
    }

    // Since `media::DataSource::Read` takes an int64_t for `pos`, need to
    // ensure we're within that.
    DCHECK(pos < static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));

    // data_source_->Read takes an integer size parameter, so must limit to
    // that.
    auto read_size = static_cast<int>(
        std::min<size_t>(size, std::numeric_limits<int>::max()));

    constexpr auto cb_adapter = [](ReadCb callback, int status) {
      if (status == media::DataSource::kReadError) {
        std::move(callback).Run(ReadStatusCodes::kError);
      } else if (status == media::DataSource::kAborted) {
        std::move(callback).Run(ReadStatusCodes::kAborted);
      } else {
        std::move(callback).Run(static_cast<size_t>(status));
      }
    };

    mb_data_source_->Read(
        pos, read_size, buffer,
        base::BindPostTask(media_task_runner_,
                           base::BindOnce(cb_adapter, std::move(callback))));
  }

  base::StringPiece GetMimeType() const override {
    return mb_data_source_->GetMimeType();
  }

 private:
  static absl::optional<uint64_t> DetermineSize(
      MultiBufferDataSource& source,
      absl::optional<media::hls::types::ByteRange> range) {
    // If we have a byterange from the manifest, go with that over
    // content-length
    if (range.has_value()) {
      return range->GetLength();
    }

    int64_t size = 0;
    if (source.GetSize(&size)) {
      return static_cast<uint64_t>(size);
    }

    return absl::nullopt;
  }

  base::WeakPtr<HlsDataSourceProviderImpl> provider_;
  std::unique_ptr<MultiBufferDataSource> mb_data_source_;
  absl::optional<media::hls::types::ByteRange> range_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
};

HlsDataSourceProviderImpl::HlsDataSourceProviderImpl(
    media::MediaLog* media_log,
    UrlIndex* url_index,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const base::TickClock* tick_clock)
    : media_log_(media_log->Clone()),
      url_index_(url_index),
      main_task_runner_(std::move(main_task_runner)),
      media_task_runner_(std::move(media_task_runner)) {
  buffered_data_source_host_ = std::make_unique<BufferedDataSourceHostImpl>(
      base::BindRepeating(&Self::NotifyDataSourceProgress,
                          base::Unretained(this)),
      tick_clock);
}

void HlsDataSourceProviderImpl::SetOwner(media::HlsDemuxer* owner) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!owner_ && owner);
  owner_ = owner;
}

void HlsDataSourceProviderImpl::RequestDataSource(
    GURL uri,
    absl::optional<media::hls::types::ByteRange> range,
    RequestCb callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(owner_);

  // TODO(https://crbug.com/1379488): Find a way to force
  // `MultiBufferDataSource` to limit its requests to within `range`.
  auto url_data =
      url_index_->GetByUrl(uri, UrlData::CORS_UNSPECIFIED, UrlIndex::kNormal);
  auto mb_data_source = std::make_unique<MultiBufferDataSource>(
      main_task_runner_, std::move(url_data), media_log_.get(),
      buffered_data_source_host_.get(),
      base::BindRepeating(&Self::NotifyDownloading, weak_factory_.GetWeakPtr(),
                          uri.spec()));

  auto* mb_data_source_ptr = mb_data_source.get();
  mb_data_source_ptr->Initialize(
      base::BindOnce(&Self::DataSourceInitialized, weak_factory_.GetWeakPtr(),
                     std::move(mb_data_source), range, std::move(callback)));
}

const std::deque<MultiBufferDataSource*>&
HlsDataSourceProviderImpl::GetActiveDataSources() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return active_data_sources_;
}

void HlsDataSourceProviderImpl::NotifyDataSourceDestroyed(
    base::PassKey<HlsDataSourceImpl>,
    std::unique_ptr<MultiBufferDataSource> data_source) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  auto iter = base::ranges::find(active_data_sources_, data_source.get());
  DCHECK(iter != active_data_sources_.end());
  active_data_sources_.erase(iter);
}

void HlsDataSourceProviderImpl::NotifyDataSourceProgress() {
  DVLOG(1) << __func__;
}

void HlsDataSourceProviderImpl::NotifyDownloading(const std::string& uri,
                                                  bool is_downloading) {
  DVLOG(1) << __func__ << "(" << uri << ", " << is_downloading << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void HlsDataSourceProviderImpl::DataSourceInitialized(
    std::unique_ptr<MultiBufferDataSource> data_source,
    absl::optional<media::hls::types::ByteRange> range,
    RequestCb callback,
    bool success) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<HlsDataSourceImpl> hls_data_source;
  if (success) {
    active_data_sources_.push_back(data_source.get());
    hls_data_source = std::make_unique<HlsDataSourceImpl>(
        weak_factory_.GetWeakPtr(), std::move(data_source), range,
        main_task_runner_, media_task_runner_);
  }

  std::move(callback).Run(std::move(hls_data_source));
}

}  // namespace blink
