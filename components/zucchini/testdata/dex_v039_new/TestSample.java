// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.zucchini.testdata;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

/** Sample DEX 039 test fixture (new version) exercising diverse DEX structures. */
@TestSample.Tag("sample-new")
public class TestSample {
    @Retention(RetentionPolicy.RUNTIME)
    @interface Tag {
        String value();
    }

    private static final String EXTRA_BANNER = "zucchini-v2";
    private static final String GREETING = "hello-new";
    private static final int BASE_OFFSET = 99;
    private static final long EXTRA_SEED = 123456789L;

    @Tag("counter-field")
    private static int sCounter = 13;

    @Tag("dispatch-method")
    public String dispatch(@Tag("mode-param") int mode, int code) {
        String prefix;
        switch (mode) {
            case 0:
                prefix = GREETING;
                break;
            case 1:
                prefix = "alpha-v2";
                break;
            case 2:
                prefix = "beta-v2";
                break;
            case 3:
                prefix = "gamma-v2";
                break;
            case 4:
                prefix = "delta-v2";
                break;
            case 5:
                prefix = EXTRA_BANNER;
                break;
            default:
                prefix = "unknown-v2";
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
            case 100000:
                return prefix + ":hundred-thousand";
            default:
                return extraAudit(prefix, sumTable(code));
        }
    }

    @Tag("audit-method")
    public String extraAudit(@Tag("label-param") String label, int value) {
        return label + "#" + (value + EXTRA_SEED);
    }

    public int sumTable(int limit) {
        int[] table = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};
        int sum = 0;
        for (int i = 0; i < table.length; i++) {
            if (i >= limit) {
                break;
            }
            sum += (i % 2 == 0) ? table[i] + 1 : table[i] * 3;
        }
        return sum + sCounter + BASE_OFFSET;
    }

    public String invokeTarget(String prefix, int value) {
        return prefix + ":" + (value + 1);
    }

    public String invokePolymorphic(int value) {
        try {
            MethodType type = MethodType.methodType(String.class, String.class, int.class);
            MethodHandle handle =
                    MethodHandles.lookup().findVirtual(TestSample.class, "invokeTarget", type);
            return (String) handle.invoke(this, "mh-new", value);
        } catch (IllegalArgumentException e) {
            return "illegal-v2:" + e.getMessage();
        } catch (Throwable t) {
            return "err-v2:" + t.getClass().getName();
        }
    }
}
