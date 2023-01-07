// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * The list of all capabilities that might be supported by the Chromoting host
 * and client. As more capabilities are supported on the android client,
 * they can be enumerated here. This list must be kept synced with the
 * Chromoting host.
 */
public class Capabilities {
    public static final String CAST_CAPABILITY = "casting";
    public static final String TOUCH_CAPABILITY = "touchEvents";
}
