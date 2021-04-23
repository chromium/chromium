// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SANDBOX_POC_POCDLL_EXPORTS_H_
#define SANDBOX_WIN_SANDBOX_POC_POCDLL_EXPORTS_H_

#include <windows.h>

#ifdef POCDLL_EXPORTS
#define POCDLL_API __declspec(dllexport) __cdecl
#else
#define POCDLL_API __declspec(dllimport) __cdecl
#endif

extern "C" {
// Tries to open several known system path and outputs
// the result.
// "log" is the handle of the log file.
void POCDLL_API TestFileSystem(HANDLE log);

// Tries to find all handles open in the process and prints the name of the
// resource references by the handle along with the access right.
// "log" is the handle of the log file.
void POCDLL_API TestGetHandle(HANDLE log);

// Creates a lot of threads until it cannot create more. The goal of this
// function is to determine if it's possible to crash the machine when we
// flood the machine with new threads
// "log" is the handle of the log file.
void POCDLL_API TestThreadBombing(HANDLE log);

// Takes all cpu of the machine. For each processor on the machine we assign
// a thread. This thread will compute a mathematical expression over and over
// to take all cpu.
// "log" is the handle of the log file.
// Note: here we are using the affinity to find out how many processors are on
// the machine and to force a thread to run only on a given processor.
void POCDLL_API TestTakeAllCpu(HANDLE log);

// Creates memory in the heap until it fails 5 times in a row and prints the
// amount of memory created. This function is used to find out if it's possible
// to take all memory on the machine and crash the system.
// "log" is the handle of the log file.
void POCDLL_API TestUseAllMemory(HANDLE log);

// Creates millions of kernel objects. This function is used to find out if it's
// possible to crash the system if we create too many kernel objects and if we
// hold too many handles. All those kernel objects are unnamed.
// "log" is the handle of the log file.
void POCDLL_API TestCreateObjects(HANDLE log);

// Receives a hwnd and tries to close it. This is the callback for EnumWindows.
// It will be called for each window(hwnd) on the system.
// "log" is the handle of the log file.
// Always returns TRUE to tell the system that we want to continue the
// enumeration.
void POCDLL_API TestCloseHWND(HANDLE log);

// Tries to listen on the port 88.
// "log" is the handle of the log file.
void POCDLL_API TestNetworkListen(HANDLE log);

// Lists all processes on the system and tries to open them
// "log" is the handle of the log file.
void POCDLL_API TestProcesses(HANDLE log);

// Lists all threads on the system and tries to open them
// "log" is the handle of the log file.
void POCDLL_API TestThreads(HANDLE log);

// Tries to open some known system registry key and outputs the result.
// "log" is the handle of the log file.
void POCDLL_API TestRegistry(HANDLE log);

// Records all keystrokes typed for 15 seconds and then display them.
// "log" is the handle of the log file.
void POCDLL_API TestSpyKeys(HANDLE log);

// Tries to read pixels on the monitor and output if the operation
// failes or succeeded.
// "log" is the handle of the log file.
void POCDLL_API TestSpyScreen(HANDLE log);

// Runs all tests except those who are invasive
void POCDLL_API Run(HANDLE log);
}

#endif  // SANDBOX_WIN_SANDBOX_POC_POCDLL_EXPORTS_H_
