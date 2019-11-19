// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MEMORY_MONITOR_H_
#define IOS_CHROME_APP_MEMORY_MONITOR_H_

// Starts the memory monitor that periodically updates the amount of free
// memory and free disk space in the background.
void StartFreeMemoryMonitor();

#endif  // IOS_CHROME_APP_MEMORY_MONITOR_H_
