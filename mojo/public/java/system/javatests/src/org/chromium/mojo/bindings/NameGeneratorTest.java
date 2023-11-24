// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.mojo.bindings.test.mojom.sample.NameGeneratorConstants;
import org.chromium.mojo.bindings.test.mojom.sample.SupportedCases;

/** Test mojom constant names generated for java. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class NameGeneratorTest {
    @Test
    @SmallTest
    public void testLowerCamelCase() {
        Assert.assertTrue(classHasField(SupportedCases.class, "LOWER_CAMEL_CASE"));
    }

    @Test
    @SmallTest
    public void testUpperCamelCase() {
        Assert.assertTrue(classHasField(SupportedCases.class, "UPPER_CAMEL_CASE"));
    }

    @Test
    @SmallTest
    public void testSnakeCase() {
        Assert.assertTrue(classHasField(SupportedCases.class, "SNAKE_CASE"));
    }

    @Test
    @SmallTest
    public void testMacroCase() {
        Assert.assertTrue(classHasField(SupportedCases.class, "MACRO_CASE"));
    }

    @Test
    @SmallTest
    public void testHungarianNotation() {
        Assert.assertTrue(classHasField(SupportedCases.class, "HUNGARIAN_NOTATION"));
    }

    @Test
    @SmallTest
    public void testUpperAcronym() {
        Assert.assertTrue(classHasField(SupportedCases.class, "UPPER_ACRONYM_CASE"));
    }

    @Test
    @SmallTest
    public void testNames() {
        Assert.assertTrue(classHasField(NameGeneratorConstants.class, "PAD_RSA_PKCS1_1_5_SIGN"));
        Assert.assertTrue(classHasField(NameGeneratorConstants.class, "DIGEST_SHA1"));
        Assert.assertTrue(classHasField(NameGeneratorConstants.class, "E2E_INTEGRATION"));
        Assert.assertTrue(classHasField(NameGeneratorConstants.class, "M3_TEST"));
        Assert.assertTrue(classHasField(NameGeneratorConstants.class, "URL_LOADER_FACTORY"));
        Assert.assertTrue(classHasField(NameGeneratorConstants.class, "IPV6_ADDRESS"));
        Assert.assertTrue(classHasField(NameGeneratorConstants.class, "NUMB3R5_IN_TH3_MIDDL3"));
        Assert.assertTrue(classHasField(NameGeneratorConstants.class, "NAME_WITH_UNDERSCORE"));
        Assert.assertTrue(classHasField(NameGeneratorConstants.class, "SINGLETON"));
    }

    private static <T> boolean classHasField(Class<T> clazz, String fieldName) {
        try {
            clazz.getField(fieldName);
            return true;
        } catch (NoSuchFieldException e) {
            return false;
        }
    }
}
