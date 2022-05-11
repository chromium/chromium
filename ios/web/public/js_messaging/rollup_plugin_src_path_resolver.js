// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Plugin for rollup to correctly resolve resources.
 */

import path from 'path'

export default function src_path_resolver(rootPath, pathMappingsJson) {
  return {
    name: 'rollup-plugin-src-path-resolver',

    resolveId(source, originScript) {
      // Allow root files to be processed directly by rollup.
      if (!originScript) {
        return null;
      }

      if (source.startsWith('//')) {
        // Strip leading double slashes
        const sourcePath = source.substr(2);

        // Check if an import exists within a dependency directory.
        const pathMappings = JSON.parse(pathMappingsJson);
        if (sourcePath in pathMappings) {
          return pathMappings[sourcePath]
        }

        // Assume file exists at root.
        return path.join(rootPath, sourcePath);
      }

      // Relative paths are acceptable from within third-party scripts.
      if (!source.startsWith('/')) {
        let fullPath = path.join(path.dirname(originScript), source);
        let relativeToRoot = path.relative(rootPath, fullPath);
        if (relativeToRoot.startsWith('third_party')) {
          return fullPath;
        }
      }

      this.error(
          `Invalid path, imports must be absolute from src root and start ` +
          `with '//'. Failed path: '${source}'`);
    },
  };
}