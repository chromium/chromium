// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SMS final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // |content| is the raw content of the SMS message.
  explicit SMS(const WTF::String& content);

  ~SMS() override;

  // Sms IDL interface.
  const String& content() const;

 private:
  const String content_;

  DISALLOW_COPY_AND_ASSIGN(SMS);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_H_
