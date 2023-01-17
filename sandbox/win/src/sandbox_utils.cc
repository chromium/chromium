// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_utils.h"

#include <windows.h>
#include <winternl.h>

#include "base/check.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

void InitObjectAttribs(const std::wstring& name,
                       ULONG attributes,
                       HANDLE root,
                       OBJECT_ATTRIBUTES* obj_attr,
                       UNICODE_STRING* uni_name,
                       SECURITY_QUALITY_OF_SERVICE* security_qos) {
  ::RtlInitUnicodeString(uni_name, name.c_str());
  InitializeObjectAttributes(obj_attr, uni_name, attributes, root, nullptr);
  obj_attr->SecurityQualityOfService = security_qos;
}

}  // namespace sandbox
