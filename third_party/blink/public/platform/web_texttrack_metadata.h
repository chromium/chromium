// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXTTRACK_METADATA_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXTTRACK_METADATA_H_

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class BLINK_PLATFORM_EXPORT TextTrackMetadata {
 public:
  TextTrackMetadata(const std::string& lang,
                    const std::string& kind,
                    const std::string& label,
                    const std::string& id)
      : language_(lang), kind_(kind), label_(label), id_(id) {}
  ~TextTrackMetadata() = default;

  const std::string& language() const { return language_; }
  const std::string& kind() const { return kind_; }
  const std::string& label() const { return label_; }
  const std::string& id() const { return id_; }

 private:
  std::string language_;
  std::string kind_;
  std::string label_;
  std::string id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXTTRACK_METADATA_H_
