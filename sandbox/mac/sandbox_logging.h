// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SANDBOX_LOGGING_H_
#define SANDBOX_MAC_SANDBOX_LOGGING_H_

namespace sandbox {

// Sandbox has its own logging implementation to avoid linking against //base.
// Sandbox should not link against libbase because libbase brings in numerous
// system libraries that increase the attack surface of the sandbox code.
namespace logging {

// The follow three functions log a format string and its arguments at
// the platform specific version of the given log levels.
void Info(const char* fmt, ...);
void Warning(const char* fmt, ...);
void Error(const char* fmt, ...);
// This logs a platform specific critical message and aborts the process.
void Fatal(const char* fmt, ...);

// The PError and PFatal functions log the errno information as well.
void PError(const char* fmt, ...);
void PFatal(const char* fmt, ...);

}  // namespace logging

}  // namespace sandbox

#endif  // SANDBOX_MAC_SANDBOX_LOGGING_H_
