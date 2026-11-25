// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.zucchini.testdata;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

/** Sample DEX 039 test fixture (old version) exercising diverse DEX structures. */
@TestSample.Tag("sample-old")
public class TestSample {
    @Retention(RetentionPolicy.RUNTIME)
    @interface Tag {
        String value();
    }

    private static final String GREETING = "hello-old";
    private static final int BASE_OFFSET = 42;

    @Tag("counter-field")
    private static int sCounter = 7;

    @Tag("dispatch-method")
    public String dispatch(@Tag("mode-param") int mode, int code) {
        String prefix;
        switch (mode) {
            case 0:
                prefix = GREETING;
                break;
            case 1:
                prefix = "alpha";
                break;
            case 2:
                prefix = "beta";
                break;
            case 3:
                prefix = "gamma";
                break;
            default:
                prefix = "unknown";
                break;
        }
        switch (code) {
            case 10:
                return prefix + ":ten";
            case 100:
                return prefix + ":hundred";
            case 1000:
                return prefix + ":thousand";
            case 10000:
                return prefix + ":ten-thousand";
            default:
                return prefix + ":" + sumTable(code);
        }
    }

    public int sumTable(int limit) {
        int[] table = {3, 1, 4, 1, 5, 9, 2, 6};
        int sum = 0;
        for (int i = 0; i < table.length; i++) {
            if (i >= limit) {
                break;
            }
            sum += (i % 2 == 0) ? table[i] : table[i] * 2;
        }
        return sum + sCounter + BASE_OFFSET;
    }

    public String invokeTarget(String prefix, int value) {
        return prefix + ":" + value;
    }

    public String invokePolymorphic(int value) {
        try {
            MethodType type = MethodType.methodType(String.class, String.class, int.class);
            MethodHandle handle =
                    MethodHandles.lookup().findVirtual(TestSample.class, "invokeTarget", type);
            return (String) handle.invoke(this, "mh-old", value);
        } catch (IllegalArgumentException e) {
            return "illegal:" + e.getMessage();
        } catch (Throwable t) {
            return "err:" + t.getClass().getName();
        }
    }
}
