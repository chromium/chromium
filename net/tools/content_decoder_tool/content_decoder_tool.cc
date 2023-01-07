// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/content_decoder_tool/content_decoder_tool.h"

#include <memory>
#include <utility>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/brotli_source_stream.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/source_stream.h"

namespace net {

namespace {

const int kBufferLen = 4096;

const char kDeflate[] = "deflate";
const char kGZip[] = "gzip";
const char kXGZip[] = "x-gzip";
const char kBrotli[] = "br";

class StdinSourceStream : public SourceStream {
 public:
  explicit StdinSourceStream(std::istream* input_stream)
      : SourceStream(SourceStream::TYPE_NONE), input_stream_(input_stream) {}

  StdinSourceStream(const StdinSourceStream&) = delete;
  StdinSourceStream& operator=(const StdinSourceStream&) = delete;

  ~StdinSourceStream() override = default;

  // SourceStream implementation.
  int Read(IOBuffer* dest_buffer,
           int buffer_size,
           CompletionOnceCallback callback) override {
    if (input_stream_->eof())
      return OK;
    if (input_stream_) {
      input_stream_->read(dest_buffer->data(), buffer_size);
      int bytes = input_stream_->gcount();
      return bytes;
    }
    return ERR_FAILED;
  }

  std::string Description() const override { return ""; }

  bool MayHaveMoreBytes() const override { return true; }

 private:
  std::istream* input_stream_;
};

}  // namespace

// static
bool ContentDecoderToolProcessInput(std::vector<std::string> content_encodings,
                                    std::istream* input_stream,
                                    std::ostream* output_stream) {
  std::unique_ptr<SourceStream> upstream(
      std::make_unique<StdinSourceStream>(input_stream));
  for (const auto& content_encoding : base::Reversed(content_encodings)) {
    std::unique_ptr<SourceStream> downstream;
    if (base::EqualsCaseInsensitiveASCII(content_encoding, kBrotli)) {
      downstream = CreateBrotliSourceStream(std::move(upstream));
    } else if (base::EqualsCaseInsensitiveASCII(content_encoding, kDeflate)) {
      downstream = GzipSourceStream::Create(std::move(upstream),
                                            SourceStream::TYPE_DEFLATE);
    } else if (base::EqualsCaseInsensitiveASCII(content_encoding, kGZip) ||
               base::EqualsCaseInsensitiveASCII(content_encoding, kXGZip)) {
      downstream = GzipSourceStream::Create(std::move(upstream),
                                            SourceStream::TYPE_GZIP);
    } else {
      LOG(ERROR) << "Unsupported decoder '" << content_encoding << "'.";
      return false;
    }
    if (downstream == nullptr) {
      LOG(ERROR) << "Couldn't create the decoder.";
      return false;
    }
    upstream = std::move(downstream);
  }
  if (!upstream) {
    LOG(ERROR) << "Couldn't create the decoder.";
    return false;
  }
  scoped_refptr<IOBuffer> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kBufferLen);
  while (true) {
    TestCompletionCallback callback;
    int bytes_read =
        upstream->Read(read_buffer.get(), kBufferLen, callback.callback());
    if (bytes_read == ERR_IO_PENDING)
      bytes_read = callback.WaitForResult();

    if (bytes_read < 0) {
      LOG(ERROR) << "Couldn't decode stdin.";
      return false;
    }
    output_stream->write(read_buffer->data(), bytes_read);
    // If EOF is read, break out the while loop.
    if (bytes_read == 0)
      break;
  }
  return true;
}

}  // namespace net
