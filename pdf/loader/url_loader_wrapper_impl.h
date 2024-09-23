// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_LOADER_URL_LOADER_WRAPPER_IMPL_H_
#define PDF_LOADER_URL_LOADER_WRAPPER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_span.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "pdf/loader/url_loader_wrapper.h"
#include "ui/gfx/range/range.h"

namespace chrome_pdf {

class UrlLoader;

class URLLoaderWrapperImpl : public URLLoaderWrapper {
 public:
  explicit URLLoaderWrapperImpl(std::unique_ptr<UrlLoader> url_loader);
  URLLoaderWrapperImpl(const URLLoaderWrapperImpl&) = delete;
  URLLoaderWrapperImpl& operator=(const URLLoaderWrapperImpl&) = delete;
  ~URLLoaderWrapperImpl() override;

  // URLLoaderWrapper overrides:
  int GetContentLength() const override;
  bool IsAcceptRangesBytes() const override;
  bool IsContentEncoded() const override;
  std::string GetContentType() const override;
  std::string GetContentDisposition() const override;
  int GetStatusCode() const override;
  bool IsMultipart() const override;
  bool GetByteRangeStart(int* start) const override;
  void Close() override;
  void OpenRange(const std::string& url,
                 const std::string& referrer_url,
                 uint32_t position,
                 uint32_t size,
                 base::OnceCallback<void(int)> callback) override;
  void ReadResponseBody(base::span<char> buffer,
                        base::OnceCallback<void(int)> callback) override;

 private:
  void SetHeadersFromLoader();
  void ParseHeaders(const std::string& response_headers);
  void DidOpen(base::OnceCallback<void(int)> callback, int32_t result);
  void DidRead(base::OnceCallback<void(int)> callback, int32_t result);

  void ReadResponseBodyImpl(base::OnceCallback<void(int)> callback);

  std::unique_ptr<UrlLoader> url_loader_;

  int content_length_ = -1;
  bool accept_ranges_bytes_ = false;
  bool content_encoded_ = false;
  std::string content_type_;
  std::string content_disposition_;
  std::string multipart_boundary_;
  gfx::Range byte_range_ = gfx::Range::InvalidRange();
  bool is_multipart_ = false;
  base::raw_span<char, DanglingUntriaged> buffer_;
  bool multi_part_processed_ = false;

  base::OneShotTimer read_starter_;

  base::WeakPtrFactory<URLLoaderWrapperImpl> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_LOADER_URL_LOADER_WRAPPER_IMPL_H_
