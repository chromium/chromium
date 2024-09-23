// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hooking_win_proc.h"

#include <windows.h>

LRESULT CALLBACK WndProc(HWND window,
                         UINT message,
                         WPARAM w_param,
                         LPARAM l_param) {
  // Injected keystroke via PostThreadMessage won't be dispatched to here.
  // Have to use SendMessage or SendInput to trigger the keyboard hook.
  switch (message) {
    case WM_KEYDOWN:
      break;
    case WM_KEYUP:
      break;
    case WM_CLOSE:
      ::DestroyWindow(window);
      break;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      break;
    default:
      return ::DefWindowProc(window, message, w_param, l_param);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE instance,
                   HINSTANCE prev_instance,
                   LPSTR cmd_line,
                   int cmd_show) {
  constexpr wchar_t winproc_class_name[] = L"myWindowClass";
  constexpr wchar_t winproc_window_name[] = L"ChromeMitigationTests";

  // The parent process should have set up this named event already.
  HANDLE event = ::OpenEventW(EVENT_MODIFY_STATE, FALSE,
                              hooking_win_proc::g_winproc_event);
  if (event == NULL || event == INVALID_HANDLE_VALUE)
    return 1;

  // Step 1: Registering the Window Class.
  WNDCLASSEX window_class;
  window_class.cbSize = sizeof(WNDCLASSEX);
  window_class.style = 0;
  window_class.lpfnWndProc = WndProc;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = instance;
  window_class.hIcon = ::LoadIcon(NULL, IDI_APPLICATION);
  window_class.hCursor = ::LoadCursor(NULL, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOWFRAME);
  window_class.lpszMenuName = NULL;
  window_class.lpszClassName = winproc_class_name;
  window_class.hIconSm = ::LoadIcon(NULL, IDI_APPLICATION);

  if (!::RegisterClassEx(&window_class))
    return 1;

  // Step 2: Create the Window.
  HWND window =
      ::CreateWindowExW(WS_EX_CLIENTEDGE, winproc_class_name,
                        winproc_window_name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 240, 120, NULL, NULL, instance, NULL);

  if (window == NULL)
    return 1;

  ::ShowWindow(window, cmd_show);
  ::UpdateWindow(window);

  // Step 3: Signal that WinProc is up and running.
  ::SetEvent(event);

  // Step 4: The Message Loop
  // WM_QUIT results in a 0 value from GetMessageW,
  // breaking the loop.
  MSG message;
  while (::GetMessageW(&message, NULL, 0, 0) > 0) {
    ::TranslateMessage(&message);
    ::DispatchMessageW(&message);
  }

  ::SetEvent(event);
  ::CloseHandle(event);

  return 0;
}
