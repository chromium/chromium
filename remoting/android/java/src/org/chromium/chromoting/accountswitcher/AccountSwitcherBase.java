// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.accountswitcher;

/**
 * This class extends the {@link AccountSwitcher} interface, and the AccountSwitcher
 * implementations (public and internal) extend this class instead of the interface. This allows
 * adding of new methods to the interface (with a concrete stub in this class) without breaking
 * compilation of a derived class that is in a separate code repository.
 */
public abstract class AccountSwitcherBase implements AccountSwitcher {
}
