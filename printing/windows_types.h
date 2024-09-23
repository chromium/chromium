// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_WINDOWS_TYPES_H_
#define PRINTING_WINDOWS_TYPES_H_

// Windows-specific defines that are relevant just to printing.  Reduces the
// need for including windows.h.
typedef struct HDC__* HDC;
typedef struct _devicemodeW DEVMODE;

#endif  // PRINTING_WINDOWS_TYPES_H_
