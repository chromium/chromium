// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import androidx.annotation.IntDef;

public class Playground {
    @IntDef({FirstIntDef.CONST_0})
    public @interface FirstIntDef {
        int CONST_0 = 0;
    }

    @IntDef({SecondIntDef.ANOTHER_0})
    public @interface SecondIntDef {
        int ANOTHER_0 = 0;
    }

    public static int test(@FirstIntDef int first, @SecondIntDef int second) {
        return first + second;
    }

    public static int swappedIndirection(@FirstIntDef int first, @SecondIntDef int second) {
        return test(second, first);
    }

    public static void main(String[] args) {
        // Used to be no error, but fixed by https://crbug.com/330149608.
        System.out.println(swappedIndirection(FirstIntDef.CONST_0, SecondIntDef.ANOTHER_0));

        // IntDef error
        // System.out.println(test(SecondIntDef.ANOTHER_0, FirstIntDef.CONST_0));
    }
}
