// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// TODO(crbug.com/40707311): This file should be generated with all permissions policy enum values.
@IntDef({PermissionsPolicyFeature.PAYMENT, PermissionsPolicyFeature.WEB_SHARE})
@Retention(RetentionPolicy.SOURCE)
public @interface PermissionsPolicyFeature {
    int PAYMENT = org.chromium.blink.mojom.PermissionsPolicyFeature.PAYMENT;
    int WEB_SHARE = org.chromium.blink.mojom.PermissionsPolicyFeature.WEB_SHARE;
}
