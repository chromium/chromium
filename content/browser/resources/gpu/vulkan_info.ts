// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VulkanInfo_Deserialize} from './vulkan_info.mojom-webui.js';

export class VulkanInfo {
  private vulkanInfo_: Record<string, unknown>;

  constructor(base64Data: string) {
    const array = Uint8Array.from(atob(base64Data), c => c.charCodeAt(0));
    const dataView = new DataView(array.buffer);
    this.vulkanInfo_ = VulkanInfo_Deserialize(dataView);
    this.beautify(this.vulkanInfo_);
  }

  private beautify(obj: Record<string, unknown>) {
    for (const key of Object.keys(obj)) {
      const value = obj[key];

      if (key === 'specVersion') {
        continue;
      }

      if (key.endsWith('Version')) {
        obj[key] = this.beautifyVersion(value as number);
        continue;
      }

      if (key === 'extensions' || key === 'instanceExtensions') {
        obj[key] = this.beautifyExtensions(
            value as Array<{extensionName: string, specVersion: string}>);
        continue;
      }

      if (key.endsWith('UUID')) {
        obj[key] = this.beautifyUuid(value as number[]);
        continue;
      }

      if (typeof value === 'bigint') {
        // JSON.stringify() doesn't support bigint.
        obj[key] = Number(value);
        continue;
      }

      if (typeof value === 'object') {
        this.beautify(value as Record<string, unknown>);
        continue;
      }
    }
  }

  private beautifyVersion(version: number): string {
    const major = version >> 22;
    const minor = (version >> 12) & 0x3ff;
    const patch = version & 0xfff;
    return `${major}.${minor}.${patch}`;
  }

  private beautifyUuid(uuid: number[]): string {
    // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    let result = '';
    for (let i = 0; i < 16; ++i) {
      const value = uuid[i]!;
      if (i === 4 || i === 6 || i === 8 || i === 10) {
        result += '-';
      }
      if (value < 0x10) {
        result += '0';
      }
      result += value.toString(16);
    }
    return result;
  }

  private beautifyExtensions(
      extensions: Array<{extensionName: string, specVersion: string}>):
      Record<string, string> {
    const result: Record<string, string> = {};
    for (const extension of extensions) {
      const name = extension['extensionName'];
      const version = extension['specVersion'];
      result[name] = version;
    }
    return result;
  }

  toString(): string {
    return JSON.stringify(this.vulkanInfo_, null, 2);
  }
}
