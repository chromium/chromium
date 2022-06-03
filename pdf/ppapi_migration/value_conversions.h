// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_VALUE_CONVERSIONS_H_
#define PDF_PPAPI_MIGRATION_VALUE_CONVERSIONS_H_

namespace base {
class Value;
}  // namespace base

namespace pp {
class Var;
}  // namespace pp

namespace chrome_pdf {

pp::Var VarFromValue(const base::Value& value);
base::Value ValueFromVar(const pp::Var& var);

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_VALUE_CONVERSIONS_H_
