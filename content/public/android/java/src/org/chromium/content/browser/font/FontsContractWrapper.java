// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.font;

import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.CancellationSignal;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.provider.FontRequest;
import androidx.core.provider.FontsContractCompat;

/** Wrapper around the {@link FontsContractCompat} static API to allow for mocking in tests. */
public class FontsContractWrapper {
    @NonNull
    FontsContractCompat.FontFamilyResult fetchFonts(
            @NonNull Context context,
            @Nullable CancellationSignal cancellationSignal,
            @NonNull FontRequest request)
            throws NameNotFoundException {
        return FontsContractCompat.fetchFonts(context, cancellationSignal, request);
    }
}
