// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include <memory>

#include "base/values.h"

namespace content {

base::Value::List GetFontList_SlowBlocking() {
  return base::Value::List();
}

}  // namespace content
