// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.json.JSONException;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.JUnitCore;
import org.junit.runner.Request;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implements;

import java.util.Collections;
import java.util.List;

/** Unit tests for RunnerFilter. */
@RunWith(BlockJUnit4ClassRunner.class)
public class TestListComputerTest {

    @Implements(TestListComputerTest.class)
    public static class Shadow {}

    @RunWith(BlockJUnit4ClassRunner.class)
    @Config(shadows = {Shadow.class})
    public static class FakeTestClass {
        @Test
        public void someTest() {}
    }

    private static void doTest(boolean allowClass) throws JSONException {
        JUnitCore core = new JUnitCore();
        TestListComputer computer =
                new TestListComputer(
                        Allowlist.fromLines(
                                "mockfile",
                                allowClass ? Collections.emptyList() : List.of("-org")));
        Class[] classes = {FakeTestClass.class};
        core.run(Request.classes(computer, classes));
        String expected =
                """
            {"configs":{"PAUSED":{"org.chromium.testing.local.TestListComputerTest$FakeTestClass":\
            ["someTest"]}},"instrumentedPackages":[],"instrumentedClasses":\
            ["org.chromium.testing.local.TestListComputerTest"]}""";
        Assert.assertEquals(expected, computer.createJson().toString());
    }

    @Test
    public void testAllowed() throws Exception {
        doTest(true);
    }

    @Test(expected = RuntimeException.class)
    public void testNotAllowed() throws Exception {
        doTest(false);
    }
}
