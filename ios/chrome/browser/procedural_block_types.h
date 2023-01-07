// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROCEDURAL_BLOCK_TYPES_H_
#define IOS_CHROME_BROWSER_PROCEDURAL_BLOCK_TYPES_H_

#import <Foundation/Foundation.h>

class GURL;

// A generic procedural block type that takes a GURL and returns nothing.
typedef void (^ProceduralBlockWithURL)(const GURL&);

// A block that takes a bool and returns nothing, as used for UIView animation
// completion.
typedef void (^ProceduralBlockWithBool)(BOOL);

#endif  // IOS_CHROME_BROWSER_PROCEDURAL_BLOCK_TYPES_H_
