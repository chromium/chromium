// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import type {AngleFeature, BrowserBridge, ClientInfo, FeatureStatus, Problem} from './browser_bridge.js';
import {getTemplate} from './info_view.html.js';
import {VulkanInfo} from './vulkan_info.js';

/**
 * Given a blob and a filename, prompts user to
 * save as a file.
 */
const saveData = (function() {
  const a = document.createElement('a');
  a.style.display = 'none';
  document.body.appendChild(a);
  return function saveData(blob: Blob, fileName: string) {
    const url = window.URL.createObjectURL(blob);
    a.href = url;
    a.download = fileName;
    a.click();
  };
}());

function getProblemTextAndUrl(problem: Problem) {
  let text = problem.description;
  let url = '';
  const pattern = ' Please update your graphics driver via this link: ';
  const pos = text.search(pattern);
  if (pos > 0) {
    url = text.substring(pos + pattern.length);
    text = text.substring(0, pos);
  }
  return {text, url};
}

function formatANGLEBug(bug: string) {
  if (bug.includes('crbug.com/')) {
    return bug.match(/\d+/)!.toString();
  } else if (bug.includes('anglebug.com/')) {
    return `anglebug:${bug.match(/\d+/)}`;
  } else {
    return bug;
  }
}

/**
 * Calls a function to insert an element between every element
 * of an existing array
 */
function insertBetweenElements<Type>(
    array: Type[], fn: (i: number) => Type): Type[] {
  const newArray = array.slice(0, 1);
  for (let i = 1; i < array.length; ++i) {
    newArray.push(fn(i), array[i] as Type);
  }
  return newArray;
}

/** Inserts <span>, </span> between every element in array */
function separateByCommas(array: HTMLElement[], comma = ', ') {
  return insertBetweenElements(array, () => createElem('span', comma));
}

/**
 * Conditionally add elements to an array
 *
 * ```js
 * const array = [
 *   "carrots",
 *   "potatoes",
 *   ...addIf(haveFruit, () => ["apple", "cherry"]),
 * ]
 * ```
 *
 * The function is not called if `cond` is false.
 */
function addIf<T>(cond: boolean, fn: () => T[]) {
  return cond ? fn() : [];
}

/**
 * Word wraps a string, prefixing each line.
 */
function wordWrap(s: string, prefix = '    ', maxLength?: number) {
  maxLength = maxLength || (80 - prefix.length);
  const lines: string[] = [];
  const words = s.split(' ');
  const line: string[] = [];
  let length = 0;
  for (const word of words) {
    if (length + word.length + 1 >= maxLength) {
      lines.push(line.join(' '));
      line.length = 0;
      length = 0;
    }
    line.push(word);
    length += word.length + 1;
  }
  if (line.length) {
    lines.push(line.join(' '));
  }
  return lines.map(s => `${prefix}${s}`).join('\n');
}

interface Attributes {
  [key: string]: string|Attributes;
}

/**
 * Creates an HTMLElement with optional attributes and children
 *
 * Examples:
 *
 * ```js
 *   br = createElem('br');
 *   p = createElem('p', 'hello world');
 *   a = createElem('a', {href: 'https://google.com', textContent: 'Google'});
 *   ul = createElement('ul', {}, [
 *     createElem('li', 'apple'),
 *     createElem('li', 'banana'),
 *   ]);
 *   h1 = createElem('h1', { style: { color: 'red' }, textContent: 'Title'})
 * ```
 */
function createElem(
    tag: string, attrs: Attributes|string = {}, children: HTMLElement[] = []) {
  const elem = document.createElement(tag) as HTMLElement;
  if (typeof attrs === 'string') {
    elem.textContent = attrs;
  } else {
    const elemAsAttribs = elem as unknown as Attributes;
    for (const [key, value] of Object.entries(attrs)) {
      if (typeof value === 'function' && key.startsWith('on')) {
        const eventName = key.substring(2).toLowerCase();
        elem.addEventListener(eventName, value, {passive: false});
      } else if (typeof value === 'object') {
        for (const [k, v] of Object.entries(value)) {
          (elemAsAttribs[key] as Attributes)[k] = v;
        }
      } else if (elemAsAttribs[key] === undefined) {
        elem.setAttribute(key, value);
      } else {
        elemAsAttribs[key] = value;
      }
    }
  }
  for (const child of children) {
    elem.appendChild(child);
  }
  return elem;
}

export interface Data {
  description: string;
  id?: string;
  value: string;
}

export interface ArrayData {
  description: string;
  value: Data[];
}

/** Creates the td elements for a table row */
function createInfoElements(
    data: Data|ArrayData, padSize: number): HTMLElement[] {
  const desc = createElem('td', {}, [
    createElem('span', data.description.padEnd(padSize)),
    createHidden(':'),
  ]);

  if (Array.isArray(data.value)) {
    return [
      desc,
      createElem('td', {}, [createInfoTable((data as ArrayData).value)]),
    ];
  } else {
    return [
      desc,
      createElem('td', {
        textContent: data.value.toString().trim(),
        id: (data as Data).id!,
      }),
    ];
  }
}

/** Creates a table from the given data */
function createInfoTable(data: Data[]|ArrayData[]) {
  const longestDesc = Math.min(
      32,
      (data as Data[])
          .reduce(
              (longest, {description}) => Math.max(longest, description.length),
              0));
  return createElem('table', {className: 'info-table'}, [
    createElem(
        'tbody', {},
        data.map(
            data => createElem('tr', {}, createInfoElements(data, longestDesc)),
            )),
  ]);
}

/**
 * Creates a hidden span that will only be used when the when
 * the user copies or downloads text.
 */
function createHidden(textContent: string) {
  return createElem('span', {className: 'copy', textContent});
}

/**
 * Given a string or Attributes returns the `textContent`
 * and the attributes with `textContent` removed
 */
function separateTextContentFromAttributes(attrs: Attributes|string = {}) {
  return typeof attrs === 'string' ? {textContent: attrs, attribs: {}} : {
    textContent: attrs['textContent'] as string || '',
    attribs: {...attrs, textContent: ''},
  };
}

/**
 * Creates a list item with a hidden `*   ` span prepended for copy
 */
function createLi(attrs: Attributes|string = {}, children: HTMLElement[] = []) {
  const {textContent, attribs} = separateTextContentFromAttributes(attrs);
  return createElem('li', attribs, [
    createHidden('*   '),
    createElem('span', textContent),
    ...children,
  ]);
}

/**
 * Creates a heading tag with hidden text for copying
 * so the copy will be like markdown.
 */
function createHeading(
    tag: string, padChar: string, attrs: Attributes|string = {},
    children: HTMLElement[] = []) {
  const {textContent, attribs} = separateTextContentFromAttributes(attrs);
  return createElem(tag, attribs, [
    createHidden('\n\n'),
    createElem('span', textContent),
    createHidden(`\n${''.padEnd(textContent.length, padChar)}`),
    ...children,
  ]);
}

/**
 * Creates a link pair with an anchor tag that is visible
 * in the page and hidden text for copying so the copy
 * will appear as (href)
 */
function createLinkPair(textContent: string, href: string) {
  return [
    createElem('a', {
      className: 'hide-on-copy',
      textContent,
      href,
    }),
    createHidden(`(${href})`),
  ];
}

/**
 * Get a string data value
 */
function getDataValue(data: Data|ArrayData): string {
  return Array.isArray(data.value) ?
      data.value.map(data => getDataValue(data)).join(',') :
      data.value;
}

/**
 * Go through Datas and find ones that start with 'GPUx'
 * return the first with who's value ends itn '*ACTIVE*'
 * or else the first one.
 * @param data
 * @returns
 */
function getActiveGPU(data: Data[]|ArrayData[]) {
  // get list of GPUs
  const gpus =
      [...data].filter(({description}) => /^GPU\d+$/.test(description));
  // get list of active GPUs
  const active = gpus.filter(data => getDataValue(data).endsWith('*ACTIVE*'));
  const all = [...active, ...gpus];
  // get the first one
  if (all.length > 0) {
    const gpu = getDataValue(all[0]!);
    const parts = gpu.split(', ')[0]!.split('=');
    return parts.length === 2 && parts[0]! === 'VENDOR' ? parseInt(parts[1]!) :
                                                          0;
  }
  return 0;
}

/** convert a value to a string or empty string if null or undefined */
function safeString(value: any) {
  return typeof value === 'undefined' || value === null ? '' : value.toString();
}

const kSections = {
  featureStatus: ['Graphics Feature Status', 'ul'],
  clientInfo: ['Version Information', 'div'],
  basicInfo: ['Driver Information', 'div'],
  workarounds: ['Driver Bug Workarounds', 'ul'],
  problems: ['Problems Detected', 'ul'],
  angleFeatures: ['ANGLE Features', 'ul'],
  dawnInfo: ['Dawn Info', 'ul'],
  compositorInfo: ['Compositor Information', 'div'],
  gpuMemoryBufferInfo: ['GpuMemoryBuffers Status', 'div'],
  displayInfo: ['Display(s) Information', 'div'],
  videoAccelerationInfo: ['Video Acceleration Information', 'div'],
  vulkanInfo: ['Vulkan Information', 'div'],
  devicePerfInfo: ['Device Performance Information', 'div'],
  diagnostics: ['Diagnostics', 'div'],
  basicInfoForHardwareGpu: ['Driver Information for Hardware GPU', 'div'],
  featureStatusForHardwareGpu:
      ['Graphics Feature Status for Hardware GPU', 'ul'],
  workaroundsForHardwareGpu: ['Driver Bug Workarounds for Hardware GPU', 'ul'],
  problemsForHardwareGpu: ['Problems Detected for Hardware GPU', 'ul'],
  logMessages: ['Log Messages', 'ul'],
} as const;

interface Section {
  div: HTMLElement;
  list: HTMLElement;
  wrap: HTMLElement;
}

type Sections = {
  [key in keyof typeof kSections]: Section
};

/**
 * @fileoverview This view displays information on the current GPU
 * hardware.  Its primary usefulness is to allow users to copy-paste
 * their data in an easy to read format for bug reports.
 */
export class InfoViewElement extends CustomElement {
  browserBridge?: BrowserBridge;
  sections?: Sections;

  static override get template() {
    return getTemplate();
  }

  addBrowserBridgeListeners(browserBridge: BrowserBridge) {
    browserBridge.addEventListener(
        'gpuInfoUpdate', this.refresh.bind(this, browserBridge));
    browserBridge.addEventListener(
        'logMessagesChange', this.refresh.bind(this, browserBridge));
    browserBridge.addEventListener(
        'clientInfoChange', this.refresh.bind(this, browserBridge));
    this.refresh(browserBridge);
  }

  /**
   * public interface for testing
   */
  getInfo(category: string, feature: string = ''): string|string[] {
    const gpuInfo = this.browserBridge?.gpuInfo;
    if (!gpuInfo) {
      throw new Error('no gpuInfo');
    }

    switch (category) {
      case 'feature-status-for-hardware-gpu-list':
        return safeString(
            gpuInfo.featureStatusForHardwareGpu?.featureStatus[feature]);
      case 'feature-status-list':
        return safeString(gpuInfo.featureStatus?.featureStatus[feature]);
      case 'active-gpu-for-hardware':
        return safeString(getActiveGPU(gpuInfo.basicInfoForHardwareGpu));
      case 'active-gpu':
        return safeString(getActiveGPU(gpuInfo.basicInfo));
      case 'workarounds':
        return (gpuInfo.featureStatus || gpuInfo.featureStatusForHardwareGpu)
                   ?.workarounds ||
            [];
      default:
        throw new Error(`unknown category: ${category}`);
    }
  }

  getSelectionText(all: boolean) {
    const dynamicStyle = this.getRequiredElement('#dynamic-style')!;
    dynamicStyle.textContent = `
      #content { white-space: pre !important; }
      .copy { display: initial; }
      .hide-on-copy { display: none; }
    `;
    const contentDiv = this.getRequiredElement('#content')!;

    // document.getSelection doesn't work through shadowDom
    // and shadowRoot getSelection is non-standard chromium
    const shadowDoc = this.shadowRoot! as unknown as Document;
    const selection = shadowDoc.getSelection()!;

    if (all) {
      selection.removeAllRanges();
      selection.selectAllChildren(contentDiv);
    } else {
      const position =
          selection.anchorNode?.compareDocumentPosition(selection.focusNode!);
      const [startNode, startOffset, endNode, endOffset] =
          ((position || 0) & Node.DOCUMENT_POSITION_FOLLOWING) ?
          [
            selection.anchorNode!,
            selection.anchorOffset,
            selection.focusNode!,
            selection.focusOffset,
          ] :
          [
            selection.focusNode!,
            selection.focusOffset,
            selection.anchorNode!,
            selection.anchorOffset,
          ];
      if (startOffset === 0) {
        // Given the selection between > and <
        //
        //   * >abc
        //   * def<
        //
        // We need to move the start of the selection back to the parent
        // otherwise the selection above will copied as
        //
        //   abc
        //   * def
        //
        // since the * (the list item's bullet) is not selectable directly.
        const li = startNode.parentElement?.closest('li');
        selection.setBaseAndExtent(
            li || startNode.parentNode!, 0, endNode, endOffset);
      }
    }

    // Get text and remove superfluous lines and whitespace.
    const text = selection.toString()
                     .replace(/\s*\n\s*\n\s*\n+/g, '\n\n')
                     .replace(/\t/g, ' ')
                     .trim();

    if (all) {
      shadowDoc.getSelection()?.removeAllRanges();
    }

    dynamicStyle.textContent = '';
    return text;
  }


  connectedCallback() {
    // Add handler to 'download report to clipboard' button
    const downloadButton = this.getRequiredElement('#download-to-file');
    assert(downloadButton);
    downloadButton.onclick = (() => {
      const text = this.getSelectionText(true);
      const blob = new Blob([text], {type: 'text/text'});
      const filename = `about-gpu-${
          new Date().toISOString().replace(/[^a-z0-9-]/ig, '-')}.txt`;
      saveData(blob, filename);
    });

    // Add a copy handler to massage the text for plain text.
    document.addEventListener('copy', (event) => {
      const text = this.getSelectionText(false);
      event!.clipboardData!.setData('text/plain', text);
      event.preventDefault();
    });

    const contentDiv = this.getRequiredElement('#content')!;
    this.sections = Object.fromEntries(Object.entries(kSections).map(
                        ([propName, [title, tag]]) => {
                          const div = createHeading('h3', '=', title);
                          const list = createElem(tag);
                          const wrap = createElem('div', {}, [
                            div,
                            list,
                          ]);
                          contentDiv.appendChild(wrap);
                          return [propName, {div, list, wrap}];
                        })) as Sections;
  }

  /**
   * Updates the view based on its currently known data
   */
  refresh(browserBridge: BrowserBridge) {
    this.browserBridge = browserBridge;
    let clientInfo: ClientInfo;
    function createSourcePermalink(
        revisionIdentifier: string, filepath: string): string {
      if (revisionIdentifier.length !== 40) {
        // If the revision id isn't a hash, just use the 0.0.0.0 version
        // from the Chrome version string "Chrome/0.0.0.0".
        revisionIdentifier = clientInfo.version.split('/')[1]!;
      }
      return `https://chromium.googlesource.com/chromium/src/+/${
          revisionIdentifier}/${filepath}`;
    }

    const sections = this.sections!;

    // Client info
    if (browserBridge.clientInfo) {
      clientInfo = browserBridge.clientInfo;

      this.setTable_(sections.clientInfo, [
        {description: 'Data exported', value: (new Date()).toISOString()},
        {description: 'Chrome version', value: clientInfo.version},
        {description: 'Operating system', value: clientInfo.operating_system},
        {
          description: 'Software rendering list URL',
          value: createSourcePermalink(
              clientInfo.revision_identifier,
              'gpu/config/software_rendering_list.json'),
        },
        {
          description: 'Driver bug list URL',
          value: createSourcePermalink(
              clientInfo.revision_identifier,
              'gpu/config/gpu_driver_bug_list.json'),
        },
        {description: 'ANGLE commit id', value: clientInfo.angle_commit_id},
        {
          description: '2D graphics backend',
          value: clientInfo.graphics_backend,
        },
        {description: 'Command Line', value: clientInfo.command_line},
      ]);
    } else {
      sections.clientInfo.list.textContent = '... loading ...';
    }

    const gpuInfo = browserBridge.gpuInfo;
    if (gpuInfo) {
      // Not using jstemplate here for blocklist status because we construct
      // href from data, which jstemplate can't seem to do.
      if (gpuInfo.featureStatus) {
        this.appendFeatureInfo_(
            gpuInfo.featureStatus, sections.featureStatus.list,
            sections.problems, sections.workarounds);
      } else {
        sections.featureStatus.list.textContent = '';
        sections.problems.list.hidden = true;
        sections.workarounds.list.hidden = true;
      }

      const hideHardware = !gpuInfo.featureStatusForHardwareGpu;
      sections.basicInfoForHardwareGpu.div.hidden = hideHardware;
      sections.featureStatusForHardwareGpu.div.hidden = hideHardware;
      sections.problemsForHardwareGpu.div.hidden = hideHardware;
      sections.workaroundsForHardwareGpu.div.hidden = hideHardware;
      if (!hideHardware) {
        this.appendFeatureInfo_(
            gpuInfo.featureStatusForHardwareGpu,
            sections.featureStatusForHardwareGpu.list,
            sections.problemsForHardwareGpu,
            sections.workaroundsForHardwareGpu);
        this.setTable_(
            sections.basicInfoForHardwareGpu, gpuInfo.basicInfoForHardwareGpu);
      }

      this.setTable_(sections.basicInfo, gpuInfo.basicInfo);
      this.setTable_(sections.compositorInfo, gpuInfo.compositorInfo);
      this.setTable_(sections.gpuMemoryBufferInfo, gpuInfo.gpuMemoryBufferInfo);
      this.setTable_(sections.displayInfo, gpuInfo.displayInfo);
      this.setTable_(
          sections.videoAccelerationInfo, gpuInfo.videoAcceleratorsInfo);

      this.updateSectionList_(
          sections.angleFeatures, gpuInfo.ANGLEFeatures,
          angleFeature => this.createAngleFeatureEl_(angleFeature));

      this.updateSection_(sections.dawnInfo, () => {
        const show = !!gpuInfo.dawnInfo && gpuInfo.dawnInfo.length > 0;
        if (show) {
          this.createDawnInfoEl_(sections.dawnInfo.list, gpuInfo.dawnInfo!);
        }
        return show;
      });

      this.updateSectionTable_(sections.diagnostics, gpuInfo.diagnostics);

      this.setTable_(
          sections.vulkanInfo,
          gpuInfo.vulkanInfo ? [{
            'description': 'info',
            'value': new VulkanInfo(gpuInfo.vulkanInfo).toString(),
            'id': 'vulkan-info-value',
          }] :
                               []);

      this.setTable_(sections.devicePerfInfo, gpuInfo.devicePerfInfo);
    } else {
      sections.basicInfo.list.textContent = '... loading ...';
      sections.diagnostics.div.hidden = true;
      sections.featureStatus.list.textContent = '';
      sections.problems.div.hidden = true;
      sections.dawnInfo.div.hidden = true;
    }

    // Log messages
    sections.logMessages.list.textContent = '';
    browserBridge.logMessages.forEach(messageObj => {
      sections.logMessages.list.appendChild(
          createElem('li', `${messageObj.header}: ${messageObj.message}`));
    });
  }

  /**
   * Clears a section and then updates it by calling fn. If fn returns false
   * it hides the section.
   */
  private updateSection_(section: Section, fn: () => boolean) {
    section.list.textContent = '';
    const show = fn();
    section.div.hidden = !show;
  }

  /**
   * Clears and and updates a section from a list. If the list is empty it
   * hides the section
   */
  private updateSectionList_<T>(
      section: Section, list: T[]|undefined, fn: (item: T) => HTMLElement) {
    this.updateSection_(section, () => {
      if (list) {
        for (const item of list) {
          section.list.appendChild(fn(item));
        }
      }
      return !!list && list.length > 0;
    });
  }

  /** Update a table, hiding it of the table has no elements */
  private updateSectionTable_(
      section: Section, inputData: Data[]|ArrayData[]|undefined) {
    this.updateSection_(section, () => {
      this.setTable_(section, inputData);
      return !!inputData && inputData.length > 0;
    });
  }

  private appendFeatureInfo_(
      featureInfo: FeatureStatus, featureStatusList: HTMLElement,
      problems: Section, workarounds: Section) {
    // Feature map
    const featureLabelMap: Record<string, string> = {
      '2d_canvas': 'Canvas',
      'gpu_compositing': 'Compositing',
      'webgl': 'WebGL',
      'multisampling': 'WebGL multisampling',
      'texture_sharing': 'Texture Sharing',
      'video_decode': 'Video Decode',
      'rasterization': 'Rasterization',
      'opengl': 'OpenGL',
      'metal': 'Metal',
      'vulkan': 'Vulkan',
      'multiple_raster_threads': 'Multiple Raster Threads',
      'native_gpu_memory_buffers': 'Native GpuMemoryBuffers',
      'protected_video_decode': 'Hardware Protected Video Decode',
      'surface_control': 'Surface Control',
      'vpx_decode': 'VPx Video Decode',
      'webgl2': 'WebGL2',
      'canvas_oop_rasterization': 'Canvas out-of-process rasterization',
      'raw_draw': 'Raw Draw',
      'video_encode': 'Video Encode',
      'direct_rendering_display_compositor':
          'Direct Rendering Display Compositor',
      'webgpu': 'WebGPU',
      'skia_graphite': 'Skia Graphite',
      'webnn': 'WebNN',
    };

    const statusMap: Record<string, {label: string, class: string}> = {
      'disabled_software': {
        'label': 'Software only. Hardware acceleration disabled',
        'class': 'feature-yellow',
      },
      'disabled_off': {'label': 'Disabled', 'class': 'feature-red'},
      'disabled_off_ok': {'label': 'Disabled', 'class': 'feature-yellow'},
      'unavailable_software': {
        'label': 'Software only, hardware acceleration unavailable',
        'class': 'feature-yellow',
      },
      'unavailable_off': {'label': 'Unavailable', 'class': 'feature-red'},
      'unavailable_off_ok': {
        'label': 'Unavailable',
        'class': 'feature-yellow',
      },
      'enabled_readback': {
        'label': 'Hardware accelerated but at reduced performance',
        'class': 'feature-yellow',
      },
      'enabled_force': {
        'label': 'Hardware accelerated on all pages',
        'class': 'feature-green',
      },
      'enabled': {'label': 'Hardware accelerated', 'class': 'feature-green'},
      'enabled_on': {'label': 'Enabled', 'class': 'feature-green'},
      'enabled_force_on': {'label': 'Force enabled', 'class': 'feature-green'},
    };

    // feature status list
    featureStatusList.textContent = '';
    for (const featureName in featureInfo.featureStatus) {
      const featureStatus = featureInfo.featureStatus[featureName]!;

      const label = featureLabelMap[featureName];
      if (!label) {
        console.info('Missing featureLabel for', featureName);
      }

      const statusInfo = statusMap[featureStatus];
      if (!statusInfo) {
        console.info('Missing status for ', featureStatus);
      }

      featureStatusList.appendChild(createLi({}, [
        createElem('span', `${label}: `),

        createElem(
            'span',
            statusInfo ? {
              textContent: statusInfo['label'],
              className: statusInfo['class'],
            } :
                         {
                           textContent: 'Unknown',
                           className: 'feature-red',
                         },
            ),
      ]));
    }

    // problems list
    this.updateSectionList_(
        problems, featureInfo.problems,
        problem => this.createProblemEl_(problem));

    // driver bug workarounds list
    this.updateSectionList_(
        workarounds, featureInfo.workarounds,
        workaround => createLi(workaround));
  }

  private createProblemEl_(problem: Problem): HTMLElement {
    const {text, url} = getProblemTextAndUrl(problem);
    return createLi({}, [
      createElem('span', text),

      // add bug separator
      ...addIf(
          problem.crBugs.length > 0, () => [createElem('span', ':\n    ')]),

      // add bugs
      ...separateByCommas(problem.crBugs.map((id) => {
        const bugId = parseInt(id);
        const href = `http://crbug.com/${bugId}`;
        return createElem('span', {}, createLinkPair(bugId.toString(), href));
      })),

      // add affectedGpuSettings
      ...addIf(
          problem.affectedGpuSettings.length > 0,
          () =>
              [createElem('br'),
               createHidden('    '),
               createElem(
                   'i', {},
                   [
                     createElem(
                         'span',
                         problem.tag === 'disabledFeatures' ?
                             'Disabled Features: ' :
                             'Applied Workarounds: '),
                     ...separateByCommas(
                         problem.affectedGpuSettings.map(
                             (textContent) => createElem('span', {
                               textContent,
                               className: problem.tag === 'disabledFeatures' ?
                                   'feature-red' :
                                   'feature-yellow',
                             }),
                             ),
                         ',\n        '),
                   ]),
    ]),

      // add driver update link
      ...addIf(
          !!url,
          () =>
              [createElem('br'),
               createHidden('    '),
               createElem(
                   'b', {className: 'bg-yellow'},
                   [
                     createElem(
                         'span', 'Please update your graphics drive via '),
                     createElem('a', {textContent: 'this link', href: url}),
                   ]),
    ]),

      // for copy spacing
      createElem('span', '\n\n'),

    ]);
  }

  private createAngleFeatureEl_(angleFeature: AngleFeature) {
    return createLi({}, [
      // Name comes first, bolded
      createElem('b', angleFeature.name),

      // If there's a category, it follows the name in parentheses
      ...addIf(
          !!angleFeature.category,
          () =>
              [createElem('span', ` (${angleFeature.category})`),
    ]),

      // If there's a bug link, try to parse the crbug/anglebug number
      ...addIf(
          !!angleFeature.bug,
          () =>
              [createElem('span', ' '),
               ...createLinkPair(
                   formatANGLEBug(angleFeature.bug), angleFeature.bug),
    ]),

      // Follow with a colon, and the status (colored)
      createElem('span', ': '),
      createElem(
          'span',
          angleFeature.status === 'enabled' ?
              {className: 'feature-green', textContent: 'Enabled'} :
              {className: 'feature-red', textContent: 'Disabled'}),

      ...addIf(
          !!angleFeature.condition,
          () =>
              [createHidden('\n    condition'),
               createElem('span', {
                 className: 'feature-gray',
                 textContent: `: ${angleFeature.condition}`,
               }),
    ]),

      ...addIf(
          !!angleFeature.description,
          () =>
              [createElem('br'),
               createElem('i', wordWrap(angleFeature.description!)),
    ]),

      // for copy spacing
      createElem('span', '\n\n'),
    ]);
  }

  private setTable_(section: Section, inputData: Data[]|ArrayData[]|undefined) {
    section.list.textContent = '';
    section.list.appendChild(createInfoTable(inputData || []));
  }

  private createDawnInfoEl_(dawnInfoList: HTMLElement, gpuDawnInfo: string[]) {
    dawnInfoList.textContent = '';
    let inProcessingToggles = false;

    for (let i = 0; i < gpuDawnInfo.length; ++i) {
      const infoString = gpuDawnInfo[i]!;
      let infoEl: HTMLElement;

      if (infoString.startsWith('<')) {
        // GPU type and backend type.
        // Add an empty line for the next adaptor.
        dawnInfoList.appendChild(createElem('br'));

        // e.g. <Discrete GPU> D3D12 backend
        infoEl = createHeading('h3', '-', infoString);
        inProcessingToggles = false;
      } else if (infoString.startsWith('[')) {
        // e.g. [Enabled Toggle Names]
        infoEl = createHeading('h4', '-', {
          className: 'dawn-info-header',
          textContent: infoString,
        });

        if (infoString === '[WebGPU Status]' ||
            infoString === '[Adapter Supported Features]') {
          inProcessingToggles = false;
        } else {
          inProcessingToggles = true;
        }
      } else if (inProcessingToggles) {
        // Each toggle takes 3 strings
        infoEl = createLi({}, [
          // The toggle name comes first, bolded.
          createElem('b', `${infoString}: \n    `),

          // URL
          ...createLinkPair(gpuDawnInfo[++i]!, gpuDawnInfo[i]!),

          // Description, italicized
          createElem('i', `:\n${wordWrap(gpuDawnInfo[++i]!)}`),

          // for copy spacing
          createElem('span', '\n\n'),
        ]);
      } else {
        // Display supported extensions
        infoEl = createLi(infoString);
      }

      dawnInfoList.appendChild(infoEl);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'info-view': InfoViewElement;
  }
}

customElements.define('info-view', InfoViewElement);
