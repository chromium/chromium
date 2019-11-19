// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_INFOBAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_INFOBAR_COMMANDS_H_

enum class InfobarType;

// TODO(crbug.com/935804): This protocol is currently only being used in the
// Infobar redesign.
@protocol InfobarCommands <NSObject>

// Displays the InfobarModal for |infobarType|.
- (void)displayModalInfobar:(InfobarType)infobarType;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_INFOBAR_COMMANDS_H_
