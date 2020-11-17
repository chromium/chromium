// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/media_list_directive.h"

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

MediaListDirective::MediaListDirective(const String& name,
                                       const String& value,
                                       ContentSecurityPolicy* policy)
    : CSPDirective(name, value, policy) {
  Vector<UChar> characters;
  value.AppendTo(characters);
  Parse(characters.data(), characters.data() + characters.size());
}

bool MediaListDirective::Allows(const String& type) const {
  return plugin_types_.Contains(type);
}

void MediaListDirective::Parse(const UChar* begin, const UChar* end) {
  // TODO(amalika): Revisit parsing algorithm. Right now plugin types are not
  // validated when they are added to m_pluginTypes.
  const UChar* position = begin;

  // 'plugin-types ____;' OR 'plugin-types;'
  if (position == end) {
    Policy()->ReportInvalidPluginTypes(String());
    return;
  }

  while (position < end) {
    // _____ OR _____mime1/mime1
    // ^        ^
    SkipWhile<UChar, IsASCIISpace>(position, end);
    if (position == end)
      return;

    // mime1/mime1 mime2/mime2
    // ^
    begin = position;
    if (!SkipExactly<UChar, IsMediaTypeCharacter>(position, end)) {
      SkipWhile<UChar, IsNotASCIISpace>(position, end);
      Policy()->ReportInvalidPluginTypes(
          String(begin, static_cast<wtf_size_t>(position - begin)));
      continue;
    }
    SkipWhile<UChar, IsMediaTypeCharacter>(position, end);

    // mime1/mime1 mime2/mime2
    //      ^
    if (!SkipExactly<UChar>(position, end, '/')) {
      SkipWhile<UChar, IsNotASCIISpace>(position, end);
      Policy()->ReportInvalidPluginTypes(
          String(begin, static_cast<wtf_size_t>(position - begin)));
      continue;
    }

    // mime1/mime1 mime2/mime2
    //       ^
    if (!SkipExactly<UChar, IsMediaTypeCharacter>(position, end)) {
      SkipWhile<UChar, IsNotASCIISpace>(position, end);
      Policy()->ReportInvalidPluginTypes(
          String(begin, static_cast<wtf_size_t>(position - begin)));
      continue;
    }
    SkipWhile<UChar, IsMediaTypeCharacter>(position, end);

    // mime1/mime1 mime2/mime2 OR mime1/mime1  OR mime1/mime1/error
    //            ^                          ^               ^
    if (position < end && IsNotASCIISpace(*position)) {
      SkipWhile<UChar, IsNotASCIISpace>(position, end);
      Policy()->ReportInvalidPluginTypes(
          String(begin, static_cast<wtf_size_t>(position - begin)));
      continue;
    }
    plugin_types_.insert(
        String(begin, static_cast<wtf_size_t>(position - begin)));

    DCHECK(position == end || IsASCIISpace(*position));
  }
}

}  // namespace blink
