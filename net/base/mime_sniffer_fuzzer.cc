// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mime_sniffer.h"

#include <stddef.h>

#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "url/gurl.h"

// Fuzzer for the two main mime sniffing functions:
// SniffMimeType and SniffMimeTypeFromLocalData.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // net::SniffMimeType DCHECKs if passed an input buffer that's too large,
  // since it's meant to be used only on the first chunk of a file that's being
  // fed into a stream. Set a max size of the input to avoid running into that
  // DCHECK.  Use 64k because that's twice the size of a typical read attempt.
  constexpr size_t kMaxSniffLength = 64 * 1024;
  static_assert(kMaxSniffLength >= net::kMaxBytesToSniff,
                "kMaxSniffLength is too small.");

  FuzzedDataProvider data_provider(data, size);

  // Divide up the input.  It's important not to pass |url_string| to the GURL
  // constructor until after the length check, to prevent the fuzzer from
  // exploring GURL space with invalid inputs.
  //
  // Max lengths of URL and type hint are arbitrary.
  std::string url_string = data_provider.ConsumeRandomLengthString(4 * 1024);
  std::string mime_type_hint = data_provider.ConsumeRandomLengthString(1024);
  net::ForceSniffFileUrlsForHtml force_sniff_file_urls_for_html =
      data_provider.ConsumeBool() ? net::ForceSniffFileUrlsForHtml::kDisabled
                                  : net::ForceSniffFileUrlsForHtml::kEnabled;

  // Do nothing if remaining input is too long. An early exit prevents the
  // fuzzer from exploring needlessly long inputs with interesting prefixes.
  if (data_provider.remaining_bytes() > kMaxSniffLength)
    return 0;

  std::string input = data_provider.ConsumeRemainingBytesAsString();

  std::string result;
  net::SniffMimeType(input, GURL(url_string), mime_type_hint,
                     force_sniff_file_urls_for_html, &result);

  net::SniffMimeTypeFromLocalData(input, &result);

  return 0;
}
