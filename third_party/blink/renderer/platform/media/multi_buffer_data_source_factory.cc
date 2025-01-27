// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/multi_buffer_data_source_factory.h"

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/types/pass_key.h"
#include "media/formats/hls/types.h"
#include "third_party/blink/renderer/platform/media/buffered_data_source_host_impl.h"
#include "third_party/blink/renderer/platform/media/multi_buffer_data_source.h"

namespace blink {

MultiBufferDataSourceFactory::~MultiBufferDataSourceFactory() = default;

MultiBufferDataSourceFactory::MultiBufferDataSourceFactory(
    media::MediaLog* media_log,
    UrlDataCb get_url_data,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    const base::TickClock* tick_clock)
    : media_log_(media_log->Clone()),
      get_url_data_(get_url_data),
      main_task_runner_(std::move(main_task_runner)) {
  buffered_data_source_host_ = std::make_unique<BufferedDataSourceHostImpl>(
      base::DoNothing(), tick_clock);
}

void MultiBufferDataSourceFactory::CreateDataSource(GURL uri,
                                                    bool ignore_cache,
                                                    DataSourceCb cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  auto download_cb =
#if DCHECK_IS_ON()
      base::BindRepeating(
          [](const std::string url, bool is_downloading) {
            DVLOG(1) << __func__ << "(" << url << ", " << is_downloading << ")";
          },
          uri.spec());
#else
      base::DoNothing();
#endif

  get_url_data_.Run(std::move(uri), ignore_cache,
                    base::BindOnce(&MultiBufferDataSourceFactory::OnUrlData,
                                   weak_factory_.GetWeakPtr(), std::move(cb),
                                   std::move(download_cb)));
}

void MultiBufferDataSourceFactory::OnUrlData(
    DataSourceCb cb,
    base::RepeatingCallback<void(bool)> download_cb,
    scoped_refptr<UrlData> data) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  std::move(cb).Run(std::make_unique<MultiBufferDataSource>(
      main_task_runner_, std::move(data), media_log_.get(),
      buffered_data_source_host_.get(), std::move(download_cb)));
}

}  // namespace blink
