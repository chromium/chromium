// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

/** This interface specifies whether custom UI or CredMan is recommended by the embedder. */
public interface CredManUiRecommender {
    /**
     * Returns a recommendation on whether to use a custom UI over CredMan calls.
     *
     * @return True only iff the browser downstream identifies an advantage of custom UI.
     */
    boolean recommendsCustomUi();
}
