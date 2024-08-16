// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEST_DATA_UTIL_H_
#define MEDIA_BASE_TEST_DATA_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"

namespace media {

// Common test results.
extern const char kFailedTitle[];
extern const char kEndedTitle[];
extern const char kErrorEventTitle[];
extern const char kErrorTitle[];

// A simple external memory wrapper around base::span for testing purposes.
struct ExternalMemoryAdapterForTesting : public DecoderBuffer::ExternalMemory {
 public:
  explicit ExternalMemoryAdapterForTesting(base::span<const uint8_t> span)
      : span_(std::move(span)) {}
  const base::span<const uint8_t> Span() const override;

 private:
  const base::span<const uint8_t> span_;
};

// Returns a file path for a file in the media/test/data directory.
base::FilePath GetTestDataFilePath(std::string_view name);

// Returns relative path for test data folder: media/test/data.
base::FilePath GetTestDataPath();

// Returns the mime type for media/test/data/<file_name>.
std::string GetMimeTypeForFile(std::string_view file_name);

// Returns a string containing key value query params in the form of:
// "key_1=value_1&key_2=value2"
std::string GetURLQueryString(const base::StringPairs& query_params);

// Reads a test file from media/test/data directory and stores it in
// a DecoderBuffer.  Use DecoderBuffer vs DataBuffer to ensure no matter
// what a test does, it's safe to use FFmpeg methods.
//
//  |name| - The name of the file.
//  |buffer| - The contents of the file.
scoped_refptr<DecoderBuffer> ReadTestDataFile(std::string_view name);

// Reads a decoder buffer from a file as well, but also sets the presentation
// timestamp on it.
scoped_refptr<DecoderBuffer> ReadTestDataFile(std::string_view name,
                                              base::TimeDelta pts);

// If the provided |key_id| is that of a test key, returns true and fills the
// |key|, otherwise returns false. If |allowRotation| is true, then other valid
// values are obtained by rotating the original key_id and key. Two overloads
// are provided, one using vectors and one using strings.
bool LookupTestKeyVector(const std::vector<uint8_t>& key_id,
                         bool allowRotation,
                         std::vector<uint8_t>* key);

bool LookupTestKeyString(std::string_view key_id,
                         bool allowRotation,
                         std::string* key);

}  // namespace media

#endif  // MEDIA_BASE_TEST_DATA_UTIL_H_
