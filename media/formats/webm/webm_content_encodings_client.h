// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_CLIENT_H_
#define MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/formats/webm/webm_content_encodings.h"
#include "media/formats/webm/webm_parser.h"

namespace media {

typedef std::vector<std::unique_ptr<ContentEncoding>> ContentEncodings;

// Parser for WebM ContentEncodings element.
class MEDIA_EXPORT WebMContentEncodingsClient : public WebMParserClient {
 public:
  explicit WebMContentEncodingsClient(MediaLog* media_log);

  WebMContentEncodingsClient(const WebMContentEncodingsClient&) = delete;
  WebMContentEncodingsClient& operator=(const WebMContentEncodingsClient&) =
      delete;

  ~WebMContentEncodingsClient() override;

  const ContentEncodings& content_encodings() const;

  // WebMParserClient methods
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;
  bool OnBinary(int id, const uint8_t* data, int size) override;

 private:
  raw_ptr<MediaLog> media_log_;
  std::unique_ptr<ContentEncoding> cur_content_encoding_;
  bool content_encryption_encountered_;
  ContentEncodings content_encodings_;

  // |content_encodings_| is ready. For debugging purpose.
  bool content_encodings_ready_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_CLIENT_H_
