// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/transport_security_state_test_util.h"

#include <iterator>
#include <string_view>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/http/transport_security_state.h"
#include "url/gurl.h"

namespace net {

namespace test_default {
#include "net/http/transport_security_state_static_unittest_default.h"
}  // namespace test_default

ScopedTransportSecurityStateSource::ScopedTransportSecurityStateSource() {
  // TODO(mattm): allow using other source?
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);
}

ScopedTransportSecurityStateSource::ScopedTransportSecurityStateSource(
    uint16_t reporting_port) {
  // TODO(mattm): allow using other source?
  const TransportSecurityStateSource* base_source = &test_default::kHSTSSource;
  std::string reporting_port_string = base::NumberToString(reporting_port);
  GURL::Replacements replace_port;
  replace_port.SetPortStr(reporting_port_string);

  const char* last_report_uri = nullptr;
  for (size_t i = 0; i < base_source->pinsets_count; ++i) {
    const auto* pinset = &base_source->pinsets[i];
    if (pinset->report_uri == kNoReportURI)
      continue;
    // Currently only one PKP report URI is supported.
    if (last_report_uri)
      DCHECK_EQ(std::string_view(last_report_uri), pinset->report_uri);
    else
      last_report_uri = pinset->report_uri;
    pkp_report_uri_ =
        GURL(pinset->report_uri).ReplaceComponents(replace_port).spec();
  }
  for (size_t i = 0; i < base_source->pinsets_count; ++i) {
    const auto* pinset = &base_source->pinsets[i];
    pinsets_.push_back({pinset->accepted_pins, pinset->rejected_pins,
                        pinset->report_uri == kNoReportURI
                            ? kNoReportURI
                            : pkp_report_uri_.c_str()});
  }

  const TransportSecurityStateSource new_source = {
      base_source->huffman_tree,   base_source->huffman_tree_size,
      base_source->preloaded_data, base_source->preloaded_bits,
      base_source->root_position,  pinsets_.data(),
      base_source->pinsets_count};

  source_ = std::make_unique<TransportSecurityStateSource>(new_source);

  SetTransportSecurityStateSourceForTesting(source_.get());
}

ScopedTransportSecurityStateSource::~ScopedTransportSecurityStateSource() {
  SetTransportSecurityStateSourceForTesting(nullptr);
}

}  // namespace net
