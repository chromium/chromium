// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Plugin for rollup to correctly resolve resources.
 */

import path from 'path'

export default function src_path_resolver(rootPath) {
  return {
    name: 'rollup-plugin-src-path-resolver',

    resolveId(source, originScript) {
      // Allow root files to be processed by rollup.
      if (!originScript) {
        return null;
      }

      if (!source.startsWith('//')) {
        this.error(
            `Invalid path, imports must be absolute from src root and start ` +
            `with '//'. Failed path: '${source}'`);
      }

      // Strip leading double slashes and combine path with root.
      return path.join(rootPath, source.substr(2));
    },
  };
}