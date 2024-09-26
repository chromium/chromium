// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runner.manipulation.Filter;
import org.junit.runners.BlockJUnit4ClassRunner;

/** Unit tests for GtestFilter. */
@RunWith(BlockJUnit4ClassRunner.class)
public class GtestFilterTest {

    private static class TestClass {}

    private static class OtherTestClass {}

    @Test
    public void testDescription() {
        Filter filterUnderTest = new GtestFilter(String.format("%s.*", TestClass.class.getName()));
        Assert.assertEquals(
                "gtest-filter: " + TestClass.class.getName() + ".*", filterUnderTest.describe());
    }

    @Test
    public void testNoFilter() {
        Filter filterUnderTest = new GtestFilter("");
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "testMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "otherTestMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(OtherTestClass.class, "testMethod")));
    }

    @Test
    public void testPositiveFilterExplicit() {
        Filter filterUnderTest =
                new GtestFilter(String.format("%s.testMethod", TestClass.class.getName()));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "testMethod")));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "otherTestMethod")));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(OtherTestClass.class, "testMethod")));
    }

    @Test
    public void testPositiveFilterClassRegex() {
        Filter filterUnderTest = new GtestFilter(String.format("%s.*", TestClass.class.getName()));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "testMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "otherTestMethod")));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(OtherTestClass.class, "testMethod")));
    }

    @Test
    public void testNegativeFilterExplicit() {
        Filter filterUnderTest =
                new GtestFilter(String.format("-%s.testMethod", TestClass.class.getName()));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "testMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "otherTestMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(OtherTestClass.class, "testMethod")));
    }

    @Test
    public void testNegativeFilterClassRegex() {
        Filter filterUnderTest = new GtestFilter(String.format("-%s.*", TestClass.class.getName()));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "testMethod")));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "otherTestMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(OtherTestClass.class, "testMethod")));
    }

    @Test
    public void testPositiveAndNegativeFilter() {
        Filter filterUnderTest =
                new GtestFilter(
                        String.format(
                                "%s.*-%s.testMethod",
                                TestClass.class.getName(), TestClass.class.getName()));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "testMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "otherTestMethod")));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(OtherTestClass.class, "testMethod")));
    }

    @Test
    public void testMultiplePositiveFilters() {
        Filter filterUnderTest =
                new GtestFilter(
                        String.format(
                                "%s.otherTestMethod:%s.otherTestMethod[1]",
                                TestClass.class.getName(), OtherTestClass.class.getName()));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "testMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "otherTestMethod")));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(OtherTestClass.class, "testMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(
                                OtherTestClass.class, "otherTestMethod[1]")));
    }

    @Test
    public void testMultipleFiltersPositiveAndNegative() {
        Filter filterUnderTest =
                new GtestFilter(
                        String.format(
                                "%s.*-%s.testMethod",
                                TestClass.class.getName(), TestClass.class.getName()));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "testMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "otherTestMethod")));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(OtherTestClass.class, "testMethod")));
    }

    @Test
    public void testMultipleNegativeFilters() {
        Filter filterUnderTest =
                new GtestFilter(
                        String.format(
                                "*-%s.otherTestMethod:%s.otherTestMethod",
                                TestClass.class.getName(), OtherTestClass.class.getName()));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "testMethod")));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(TestClass.class, "otherTestMethod")));
        Assert.assertTrue(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(OtherTestClass.class, "testMethod")));
        Assert.assertFalse(
                filterUnderTest.shouldRun(
                        Description.createTestDescription(
                                OtherTestClass.class, "otherTestMethod")));
    }
}
