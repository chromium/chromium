// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import org.chromium.build.annotations.NullMarked;

/**
 * Exception that raised when NdefMessage is found to be invalid during conversion to NdefMessage.
 */
@NullMarked
public final class InvalidNdefMessageException extends Exception {}
