// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file defines the symbols from OpenSLES that android is using. It then
// loads the library dynamically on first use.

// The openSLES API is using constant as part of the API. This file will define
// proxies for those constants and redefine those when the library is first
// loaded. For this, it need to be able to change their content and so import
// the headers without const.  This is correct because OpenSLES.h is a C API.

// We include stdint.h here as a workaround for an issue caused by the
// #define const below. The inclusion of OpenSLES headers on newer Android NDK
// versions causes stdint.h to be included, which in turn includes __config.
// This causes the declaration of __sanitizer_annotate_contiguous_container to
// not use const parameters, which causes compile issues when building with
// asan. Including here forces __config to be included while const is still
// untouched.
#include <stdint.h>

#define const
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#undef const

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"

// The constants used in chromium. SLInterfaceID is actually a pointer to
// SLInterfaceID_. Those symbols are defined as extern symbols in the OpenSLES
// headers. They will be initialized to their correct values when the library is
// loaded.
SLInterfaceID SL_IID_ENGINE = NULL;
SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE = NULL;
SLInterfaceID SL_IID_ANDROIDCONFIGURATION = NULL;
SLInterfaceID SL_IID_RECORD = NULL;
SLInterfaceID SL_IID_BUFFERQUEUE = NULL;
SLInterfaceID SL_IID_VOLUME = NULL;
SLInterfaceID SL_IID_PLAY = NULL;

namespace {

// The name of the library to load.
const char kOpenSLLibraryName[] = "libOpenSLES.so";

// Loads the OpenSLES library, and initializes all the proxies.
base::NativeLibrary IntializeLibraryHandle() {
  base::NativeLibrary handle =
      base::LoadNativeLibrary(base::FilePath(kOpenSLLibraryName), NULL);
  if (!handle) {
    DLOG(ERROR) << "Unable to load " << kOpenSLLibraryName;
    return NULL;
  }

  // Setup the proxy for each symbol.
  // Attach the symbol name to the proxy address.
  struct SymbolDefinition {
    const char* name;
    SLInterfaceID* sl_iid;
  };

  // The list of defined symbols.
  const SymbolDefinition kSymbols[] = {
      {"SL_IID_ENGINE", &SL_IID_ENGINE},
      {"SL_IID_ANDROIDSIMPLEBUFFERQUEUE", &SL_IID_ANDROIDSIMPLEBUFFERQUEUE},
      {"SL_IID_ANDROIDCONFIGURATION", &SL_IID_ANDROIDCONFIGURATION},
      {"SL_IID_RECORD", &SL_IID_RECORD},
      {"SL_IID_BUFFERQUEUE", &SL_IID_BUFFERQUEUE},
      {"SL_IID_VOLUME", &SL_IID_VOLUME},
      {"SL_IID_PLAY", &SL_IID_PLAY}};

  for (size_t i = 0; i < sizeof(kSymbols) / sizeof(kSymbols[0]); ++i) {
    void* func_ptr =
        base::GetFunctionPointerFromNativeLibrary(handle, kSymbols[i].name);
    if (!func_ptr) {
      DLOG(ERROR) << "Unable to find symbol for " << kSymbols[i].name;
      return NULL;
    }
    memcpy(kSymbols[i].sl_iid, func_ptr, sizeof(SLInterfaceID));
  }
  return handle;
}

// Returns the handler to the shared library. The library itself will be lazily
// loaded during the first call to this function.
base::NativeLibrary LibraryHandle() {
  // The handle is lazily initialized on the first call.
  static base::NativeLibrary g_handle = IntializeLibraryHandle();
  return g_handle;
}

}  // namespace

// Redefine slCreateEngine symbol.
SLresult slCreateEngine(SLObjectItf* engine,
                        SLuint32 num_options,
                        SLEngineOption* engine_options,
                        SLuint32 num_interfaces,
                        SLInterfaceID* interface_ids,
                        SLboolean* interfaces_required) {
  typedef SLresult (*SlCreateEngineSignature)(SLObjectItf*, SLuint32,
                                              SLEngineOption*, SLuint32,
                                              SLInterfaceID*, SLboolean*);
  base::NativeLibrary handle = LibraryHandle();
  if (!handle)
    return SL_RESULT_INTERNAL_ERROR;

  static SlCreateEngineSignature g_sl_create_engine_handle =
      reinterpret_cast<SlCreateEngineSignature>(
          base::GetFunctionPointerFromNativeLibrary(handle, "slCreateEngine"));
  if (!g_sl_create_engine_handle) {
    DLOG(ERROR) << "Unable to find symbol for slCreateEngine";
    return SL_RESULT_INTERNAL_ERROR;
  }

  return g_sl_create_engine_handle(engine, num_options, engine_options,
                                   num_interfaces, interface_ids,
                                   interfaces_required);
}
