// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "sandbox/win/sandbox_poc/pocdll/exports.h"
#include "sandbox/win/sandbox_poc/pocdll/utils.h"

// This file contains the tests used to verify the security of the system by
// using some spying techniques.

void POCDLL_API TestSpyKeys(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  if (RegisterHotKey(NULL, 1, 0, 0x42)) {
    fprintf(output, "[GRANTED] successfully registered hotkey\r\n");
    UnregisterHotKey(NULL, 1);
  } else {
    fprintf(output, "[BLOCKED] Failed to register hotkey. Error = %ld\r\n",
            ::GetLastError());
  }

  fprintf(output, "[INFO] Logging keystrokes for 15 seconds\r\n");
  fflush(output);
  std::wstring logged;
  DWORD tick = ::GetTickCount() + 15000;
  while (tick > ::GetTickCount()) {
    for (int i = 0; i < 256; ++i) {
      if (::GetAsyncKeyState(i) & 1) {
        if (i >=  VK_SPACE && i <= 0x5A /*VK_Z*/) {
          logged.append(1, static_cast<wchar_t>(i));
        } else {
          logged.append(1, '?');
        }
      }
    }
  }

  if (logged.size()) {
    fprintf(output, "[GRANTED] Spyed keystrokes \"%S\"\r\n",
            logged.c_str());
  } else {
    fprintf(output, "[BLOCKED] Spyed keystrokes \"(null)\"\r\n");
  }
}

void POCDLL_API TestSpyScreen(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  HDC screen_dc = ::GetDC(NULL);
  COLORREF pixel_color = ::GetPixel(screen_dc, 0, 0);

  for (int x = 0; x < 10; ++x) {
    for (int y = 0; y < 10; ++y) {
      if (::GetPixel(screen_dc, x, y) != pixel_color) {
        fprintf(output, "[GRANTED] Read pixel on screen\r\n");
        return;
      }
    }
  }

  fprintf(output, "[BLOCKED] Read pixel on screen. Error = %ld\r\n",
          ::GetLastError());
}
