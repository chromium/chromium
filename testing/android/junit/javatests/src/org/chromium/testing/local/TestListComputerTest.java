// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.json.JSONException;
import org.junit.Assert;
import org.junit.Ignore;
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
        @Config(qualifiers = "sw600dp")
        public void someTest() {}
    }

    public static class DisabledTestClass {
        @Test
        @Ignore
        public void ignoredTest() {}
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
                {"configs":{"PAUSED.sw600dp":{"org.chromium.testing.local.TestListComputerTest$FakeTestClass":\
                ["someTest"]}},"disabled":{},"instrumentedPackages":[],"instrumentedClasses":\
                ["org.chromium.testing.local.TestListComputerTest"]}\
                """;
        Assert.assertEquals(expected, computer.createJson().toString());
    }

    @Test
    public void testAllowed() throws Exception {
        doTest(true);
    }

    @Test
    public void testDisabled() throws Exception {
        JUnitCore core = new JUnitCore();
        TestListComputer computer =
                new TestListComputer(Allowlist.fromLines("mockfile", Collections.emptyList()));
        Class[] classes = {DisabledTestClass.class};
        core.run(Request.classes(computer, classes));
        String expected =
                """
                {"configs":{},"disabled":{"PAUSED":{"org.chromium.testing.local.TestListComputerTest$DisabledTestClass":\
                ["ignoredTest"]}},"instrumentedPackages":[],"instrumentedClasses":[]}\
                """;
        Assert.assertEquals(expected, computer.createJson().toString());
    }

    @Test(expected = RuntimeException.class)
    public void testNotAllowed() throws Exception {
        doTest(false);
    }
}
