// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/toggle_root.h"

namespace blink {

String ToggleRoot::State::ToString() const {
  switch (GetType()) {
    case Type::Integer:
      return String::Number(AsInteger());
    case Type::Name:
      return AsName().GetString();
  }
}

}  // namespace blink
