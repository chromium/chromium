// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.window.InputTransferToken;

/**
 * A wrapper for sending InputTransferToken over Binder. InputTransferToken is parcelable itself but
 * we can't directly pass it over since it was only introduced in SDK level 35 and the java compiler
 * complains about "Field requires API level 35" in the stub generated for aidl file.
 */
public class InputTransferTokenWrapper implements Parcelable {
    private InputTransferToken mToken;

    public InputTransferTokenWrapper(InputTransferToken token) {
        assert token != null;
        mToken = token;
    }

    public InputTransferToken getInputTransferToken() {
        return mToken;
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(/* hasToken= */ 1);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            mToken.writeToParcel(out, flags);
        }
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public static final Parcelable.Creator<InputTransferTokenWrapper> CREATOR =
            new Parcelable.Creator<InputTransferTokenWrapper>() {
                @Override
                public InputTransferTokenWrapper createFromParcel(Parcel in) {
                    final boolean hasToken = (in.readInt() == 1);
                    if (hasToken
                            && Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
                        InputTransferToken token = InputTransferToken.CREATOR.createFromParcel(in);
                        return new InputTransferTokenWrapper(token);
                    } else {
                        // The only constructor of class assert's token is non-null and
                        // InputTransferToken can only be non-null on V+ devices. Since the object
                        // was already created it ensures this conditions shouldn't be reached.
                        throw new RuntimeException("not reached");
                    }
                }

                @Override
                public InputTransferTokenWrapper[] newArray(int size) {
                    return new InputTransferTokenWrapper[size];
                }
            };
}
