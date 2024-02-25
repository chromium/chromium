// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.service_manager;

import org.chromium.mojo.bindings.Interface;

/**
 * A factory that creates implementations of a mojo interface.
 *
 * @param <I> the mojo interface
 */
public interface InterfaceFactory<I extends Interface> {
    /** Returns an implementation of the mojo interface. */
    I createImpl();
}
