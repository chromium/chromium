// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_GC_PLUGIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_GC_PLUGIN_H_

#if defined(__clang__)
// GC_PLUGIN_IGNORE is used to make the Blink GC plugin ignore a particular
// class or field when checking for proper usage.  When using GC_PLUGIN_IGNORE a
// reason must be provided as an argument. In most cases this will be a bug id
// where the bug describes what needs to happen to remove the GC_PLUGIN_IGNORE
// again.
//
// Developer note: this macro must be kept in sync with the definition of
// STACK_ALLOCATED_IGNORE in /base/memory/stack_allocated.h.
#define GC_PLUGIN_IGNORE(reason) \
  __attribute__((annotate("blink_gc_plugin_ignore")))
// GC_PLUGIN_IGNORE_FILE is used to make the Blink GC plugin ignore a whole
// file. All classes, fields, methods and variables in that file will be skipped
// by the plugin. Any incorrect usages in the file will not be reported by the
// plugin.
//
// Always prefer using GC_PLUGIN_IGNORE over GC_PLUGIN_IGNORE_FILE.
// GC_PLUGIN_IGNORE_FILE should only be used when GC_PLUGIN_IGNORE is not
// appropriate (e.g. when using GC_PLUGIN_IGNORE results in binary size
// regressions, etc.).
#define GC_PLUGIN_IGNORE_FILE(reason)                           \
  _Pragma("clang diagnostic push")                              \
      _Pragma("clang diagnostic ignored \"-Wunknown-pragmas\"") \
          _Pragma("blink_gc_plugin_ignore_file")                \
              _Pragma("clang diagnostic pop")
#else  // !defined(__clang__)
#define GC_PLUGIN_IGNORE(reason)
#define GC_PLUGIN_IGNORE_FILE(reason)
#endif  // !defined(__clang__)

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_GC_PLUGIN_H_
