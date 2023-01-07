// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_WRAPPERS_H
#define CRAZY_LINKER_WRAPPERS_H

namespace crazy {

// If |symbol_name| is the name of a linker-specific symbol name, like
// 'dlopen' or 'dlsym', return the address of the corresponding wrapper.
// Return NULL otherwise.
void* WrapLinkerSymbol(const char* symbol_name);

}  // namespace crazy

#endif  // CRAZY_LINKER_WRAPPERS_H
