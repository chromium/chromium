// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.font;

import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.CancellationSignal;

import androidx.core.provider.FontRequest;
import androidx.core.provider.FontsContractCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Wrapper around the {@link FontsContractCompat} static API to allow for mocking in tests. */
@NullMarked
public class FontsContractWrapper {

    FontsContractCompat.FontFamilyResult fetchFonts(
            Context context, @Nullable CancellationSignal cancellationSignal, FontRequest request)
            throws NameNotFoundException {
        return FontsContractCompat.fetchFonts(context, cancellationSignal, request);
    }
}
