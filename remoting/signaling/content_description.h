// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_CONTENT_DESCRIPTION_H_
#define REMOTING_SIGNALING_CONTENT_DESCRIPTION_H_

#include <memory>
#include <string>

#include "remoting/signaling/jingle_data_structures.h"

namespace remoting {

class ContentDescription {
 public:
  static const char kChromotingContentName[];

  explicit ContentDescription(const JingleAuthentication& authentication);

  ContentDescription(const ContentDescription&);
  ContentDescription& operator=(const ContentDescription&) = delete;

  ~ContentDescription();

  std::unique_ptr<ContentDescription> Clone() const;

  const JingleAuthentication& authentication() const { return authentication_; }

 private:
  JingleAuthentication authentication_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_CONTENT_DESCRIPTION_H_
