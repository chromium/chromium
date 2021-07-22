/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.support.test.internal.runner;

import android.support.annotation.VisibleForTesting;
import dalvik.system.DexFile;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * Finds class entries in apks.
 * <p/>
 * Adapted from tools/tradefederation/..ClassPathScanner
 */
@VisibleForTesting
public class ClassPathScanner {

    /**
     * A filter for classpath entry paths
     * <p/>
     * Patterned after {@link java.io.FileFilter}
     */
    public static interface ClassNameFilter {
        /**
         * Tests whether or not the specified abstract pathname should be included in a class path
         * entry list.
         *
         * @param className the relative path of the class path entry
         */
        boolean accept(String className);
    }

    /**
     * A {@link ClassNameFilter} that accepts all class names.
     */
    public static class AcceptAllFilter implements ClassNameFilter {

        /**
         * {@inheritDoc}
         */
        @Override
        public boolean accept(String className) {
            return true;
        }

    }

    /**
     * A {@link ClassNameFilter} that chains one or more filters together
     */
    public static class ChainedClassNameFilter implements ClassNameFilter {
        private final List<ClassNameFilter> mFilters = new ArrayList<ClassNameFilter>();

        public void add(ClassNameFilter filter) {
            mFilters.add(filter);
        }

        public void addAll(ClassNameFilter... filters) {
            mFilters.addAll(Arrays.asList(filters));
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public boolean accept(String className) {
            for (ClassNameFilter filter : mFilters) {
                if (!filter.accept(className)) {
                    return false;
                }
            }
            return true;
        }
    }

    /**
     * A {@link ClassNameFilter} that rejects inner classes.
     */
    public static class ExternalClassNameFilter implements ClassNameFilter {
        /**
         * {@inheritDoc}
         */
        @Override
        public boolean accept(String pathName) {
            return !pathName.contains("$");
        }
    }

    /**
     * A {@link ClassNameFilter} that only accepts package names within the given namespace.
     */
    public static class InclusivePackageNameFilter implements ClassNameFilter {

        private final String mPkgName;

        InclusivePackageNameFilter(String pkgName) {
            if (!pkgName.endsWith(".")) {
                mPkgName = String.format("%s.", pkgName);
            } else {
                mPkgName = pkgName;
            }
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public boolean accept(String pathName) {
            return pathName.startsWith(mPkgName);
        }
    }

    /**
     * A {@link ClassNameFilter} that only rejects a given package names within the given namespace.
     */
    public static class ExcludePackageNameFilter implements ClassNameFilter {

        private final String mPkgName;

        ExcludePackageNameFilter(String pkgName) {
            if (!pkgName.endsWith(".")) {
                mPkgName = String.format("%s.", pkgName);
            } else {
                mPkgName = pkgName;
            }
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public boolean accept(String pathName) {
            return !pathName.startsWith(mPkgName);
        }
    }

    private Set<String> mApkPaths = new HashSet<String>();

    public ClassPathScanner(String... apkPaths) {
        this(Arrays.asList(apkPaths));
    }

    public ClassPathScanner(Collection<String> apkPaths) {
        mApkPaths.addAll(apkPaths);
    }

    /**
     * Gets the names of all entries contained in given apk file, that match given filter.
     * @throws IOException
     */
    private void addEntriesFromApk(Set<String> entryNames, String apkPath, ClassNameFilter filter)
            throws IOException {
        DexFile dexFile = null;
        try {
            dexFile = new DexFile(apkPath);
            Enumeration<String> apkClassNames = getDexEntries(dexFile);
            while (apkClassNames.hasMoreElements()) {
                String apkClassName = apkClassNames.nextElement();
                if (filter.accept(apkClassName)) {
                    entryNames.add(apkClassName);
                }
            }
        } finally {
            if (dexFile != null) {
                dexFile.close();
            }
        }
    }

    /**
     * Retrieves the entry names from given {@link DexFile}.
     *
     * @param dexFile
     * @return {@link Enumeration} of {@link String}s
     */
    // Visible for testing
    Enumeration<String> getDexEntries(DexFile dexFile) {
        return dexFile.entries();
    }

    /**
     * Retrieves set of classpath entries that match given {@link ClassNameFilter}.
     * @throws IOException
     */
    public Set<String> getClassPathEntries(ClassNameFilter filter) throws IOException {
        // use LinkedHashSet for predictable order
        Set<String> entryNames = new LinkedHashSet<String>();
        for (String apkPath : mApkPaths) {
            addEntriesFromApk(entryNames, apkPath, filter);
        }
        return entryNames;
    }
}
