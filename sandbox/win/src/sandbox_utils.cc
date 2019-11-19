// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_utils.h"

#include <windows.h>

#include "base/logging.h"
#include "sandbox/win/src/internal_types.h"

namespace sandbox {

void InitObjectAttribs(const std::wstring& name,
                       ULONG attributes,
                       HANDLE root,
                       OBJECT_ATTRIBUTES* obj_attr,
                       UNICODE_STRING* uni_name,
                       SECURITY_QUALITY_OF_SERVICE* security_qos) {
  static RtlInitUnicodeStringFunction RtlInitUnicodeString;
  if (!RtlInitUnicodeString) {
    HMODULE ntdll = ::GetModuleHandle(kNtdllName);
    RtlInitUnicodeString = reinterpret_cast<RtlInitUnicodeStringFunction>(
        GetProcAddress(ntdll, "RtlInitUnicodeString"));
    DCHECK(RtlInitUnicodeString);
  }
  RtlInitUnicodeString(uni_name, name.c_str());
  InitializeObjectAttributes(obj_attr, uni_name, attributes, root, nullptr);
  obj_attr->SecurityQualityOfService = security_qos;
}

}  // namespace sandbox
