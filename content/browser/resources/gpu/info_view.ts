// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_view_table.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {AngleFeature, BrowserBridge, ClientInfo, FeatureStatus, Problem} from './browser_bridge.js';
import {getTemplate} from './info_view.html.js';
import {ArrayData, Data} from './info_view_table_row.js';
import {VulkanInfo} from './vulkan_info.js';

/**
 * @fileoverview This view displays information on the current GPU
 * hardware.  Its primary usefulness is to allow users to copy-paste
 * their data in an easy to read format for bug reports.
 */
export class InfoViewElement extends CustomElement {
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

  connectedCallback() {
    // Add handler to 'copy to clipboard' button
    const copyButton =
        this.shadowRoot!.querySelector<HTMLElement>('#copy-to-clipboard');
    assert(copyButton);
    copyButton.onclick = (() => {
      // Make sure nothing is selected
      const s = window.getSelection()!;
      s.removeAllRanges();
      s.selectAllChildren(this.shadowRoot!);
      document.execCommand('copy');

      // And deselect everything at the end.
      window.getSelection()!.removeAllRanges();
    });
  }

  /**
   * Updates the view based on its currently known data
   */
  refresh(browserBridge: BrowserBridge) {
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

    // Client info
    if (browserBridge.clientInfo) {
      clientInfo = browserBridge.clientInfo;

      this.setTable_('client-info', [
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
      this.setText_('client-info', '... loading...');
    }


    // GPU info, basic
    const diagnosticsDiv = this.getRequiredElement('.diagnostics');
    const diagnosticsLoadingDiv =
        this.getRequiredElement('.diagnostics-loading');
    const featureStatusList = this.getRequiredElement('.feature-status-list');
    const problemsDiv = this.getRequiredElement('.problems-div');
    const problemsList = this.getRequiredElement('.problems-list');
    const workaroundsDiv = this.getRequiredElement('.workarounds-div');
    const workaroundsList = this.getRequiredElement('.workarounds-list');
    const angleFeaturesDiv = this.getRequiredElement('.angle-features-div');
    const angleFeaturesList = this.getRequiredElement('.angle-features-list');
    const dawnInfoDiv = this.getRequiredElement('.dawn-info-div');
    const dawnInfoList = this.getRequiredElement('.dawn-info-list');

    const basicInfoForHardwareGpuDiv =
        this.getRequiredElement('.basic-info-for-hardware-gpu-div');
    const featureStatusForHardwareGpuDiv =
        this.getRequiredElement('.feature-status-for-hardware-gpu-div');
    const featureStatusForHardwareGpuList =
        this.getRequiredElement('.feature-status-for-hardware-gpu-list');
    const problemsForHardwareGpuDiv =
        this.getRequiredElement('.problems-for-hardware-gpu-div');
    const problemsForHardwareGpuList =
        this.getRequiredElement('.problems-for-hardware-gpu-list');
    const workaroundsForHardwareGpuDiv =
        this.getRequiredElement('.workarounds-for-hardware-gpu-div');
    const workaroundsForHardwareGpuList =
        this.getRequiredElement('.workarounds-for-hardware-gpu-list');

    const gpuInfo = browserBridge.gpuInfo;
    if (gpuInfo) {
      // Not using jstemplate here for blocklist status because we construct
      // href from data, which jstemplate can't seem to do.
      if (gpuInfo.featureStatus) {
        this.appendFeatureInfo_(
            gpuInfo.featureStatus, featureStatusList, problemsDiv, problemsList,
            workaroundsDiv, workaroundsList);
      } else {
        featureStatusList.textContent = '';
        problemsList.hidden = true;
        workaroundsList.hidden = true;
      }

      if (gpuInfo.featureStatusForHardwareGpu) {
        basicInfoForHardwareGpuDiv.hidden = false;
        featureStatusForHardwareGpuDiv.hidden = false;
        problemsForHardwareGpuDiv.hidden = false;
        workaroundsForHardwareGpuDiv.hidden = false;
        this.appendFeatureInfo_(
            gpuInfo.featureStatusForHardwareGpu,
            featureStatusForHardwareGpuList, problemsForHardwareGpuDiv,
            problemsForHardwareGpuList, workaroundsForHardwareGpuDiv,
            workaroundsForHardwareGpuList);
        if (gpuInfo.basicInfoForHardwareGpu) {
          this.setTable_(
              'basic-info-for-hardware-gpu', gpuInfo.basicInfoForHardwareGpu);
        } else {
          this.setTable_('basic-info-for-hardware-gpu', []);
        }
      } else {
        basicInfoForHardwareGpuDiv.hidden = true;
        featureStatusForHardwareGpuDiv.hidden = true;
        problemsForHardwareGpuDiv.hidden = true;
        workaroundsForHardwareGpuDiv.hidden = true;
      }

      if (gpuInfo.basicInfo) {
        this.setTable_('basic-info', gpuInfo.basicInfo);
      } else {
        this.setTable_('basic-info', []);
      }

      if (gpuInfo.compositorInfo) {
        this.setTable_('compositor-info', gpuInfo.compositorInfo);
      } else {
        this.setTable_('compositor-info', []);
      }

      if (gpuInfo.gpuMemoryBufferInfo) {
        this.setTable_('gpu-memory-buffer-info', gpuInfo.gpuMemoryBufferInfo);
      } else {
        this.setTable_('gpu-memory-buffer-info', []);
      }

      if (gpuInfo.displayInfo) {
        this.setTable_('display-info', gpuInfo.displayInfo);
      } else {
        this.setTable_('display-info', []);
      }

      if (gpuInfo.videoAcceleratorsInfo) {
        this.setTable_(
            'video-acceleration-info', gpuInfo.videoAcceleratorsInfo);
      } else {
        this.setTable_('video-acceleration-info', []);
      }

      if (gpuInfo.ANGLEFeatures) {
        if (gpuInfo.ANGLEFeatures.length) {
          angleFeaturesDiv.hidden = false;
          angleFeaturesList.textContent = '';
          for (const angleFeature of gpuInfo.ANGLEFeatures) {
            const angleFeatureEl = this.createAngleFeatureEl_(angleFeature);
            angleFeaturesList.appendChild(angleFeatureEl);
          }
        } else {
          angleFeaturesDiv.hidden = true;
        }
      }

      if (gpuInfo.dawnInfo) {
        if (gpuInfo.dawnInfo.length) {
          dawnInfoDiv.hidden = false;
          this.createDawnInfoEl_(dawnInfoList, gpuInfo.dawnInfo);
        } else {
          dawnInfoDiv.hidden = true;
        }
      }

      if (gpuInfo.diagnostics) {
        diagnosticsDiv.hidden = false;
        diagnosticsLoadingDiv.hidden = true;
        this.getRequiredElement('#diagnostics-table').hidden = false;
        this.setTable_('diagnostics-table', gpuInfo.diagnostics);
      } else if (gpuInfo.diagnostics === null) {
        // gpu_internals.cc sets diagnostics to null when it is being loaded
        diagnosticsDiv.hidden = false;
        diagnosticsLoadingDiv.hidden = false;
        this.getRequiredElement('#diagnostics-table').hidden = true;
      } else {
        diagnosticsDiv.hidden = true;
      }

      if (gpuInfo.vulkanInfo) {
        const vulkanInfo = new VulkanInfo(gpuInfo.vulkanInfo);
        const data = [{
          'description': 'info',
          'value': vulkanInfo.toString(),
          'id': 'vulkan-info-value',
        }];
        this.setTable_('vulkan-info', data);
      } else {
        this.setTable_('vulkan-info', []);
      }

      if (gpuInfo.devicePerfInfo) {
        this.setTable_('device-perf-info', gpuInfo.devicePerfInfo);
      } else {
        this.setTable_('device-perf-info', []);
      }
    } else {
      this.setText_('basic-info', '... loading ...');
      diagnosticsDiv.hidden = true;
      featureStatusList.textContent = '';
      problemsDiv.hidden = true;
      dawnInfoDiv.hidden = true;
    }

    // Log messages
    const messageList = this.getRequiredElement('#log-messages > ul');
    messageList.innerHTML = window.trustedTypes!.emptyHTML;
    browserBridge.logMessages.forEach(messageObj => {
      const messageEl = document.createElement('span');
      messageEl.textContent = `${messageObj.header}: ${messageObj.message}`;
      const li = document.createElement('li');
      li.appendChild(messageEl);
      messageList.appendChild(li);
    });
  }

  private appendFeatureInfo_(
      featureInfo: FeatureStatus, featureStatusList: HTMLElement,
      problemsDiv: HTMLElement, problemsList: HTMLElement,
      workaroundsDiv: HTMLElement, workaroundsList: HTMLElement) {
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
      'unavailable_off_ok': {'label': 'Unavailable', 'class': 'feature-yellow'},
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
      const featureEl = document.createElement('li');

      const nameEl = document.createElement('span');
      if (!featureLabelMap[featureName]) {
        console.info('Missing featureLabel for', featureName);
      }
      nameEl.textContent = featureLabelMap[featureName] + ': ';
      featureEl.appendChild(nameEl);

      const statusEl = document.createElement('span');
      const statusInfo = statusMap[featureStatus];
      if (!statusInfo) {
        console.info('Missing status for ', featureStatus);
        statusEl.textContent = 'Unknown';
        statusEl.className = 'feature-red';
      } else {
        statusEl.textContent = statusInfo['label'];
        statusEl.className = statusInfo['class'];
      }
      featureEl.appendChild(statusEl);

      featureStatusList.appendChild(featureEl);
    }

    // problems list
    if (featureInfo.problems.length) {
      problemsDiv.hidden = false;
      problemsList.textContent = '';
      for (const problem of featureInfo.problems) {
        const problemEl = this.createProblemEl_(problem);
        problemsList.appendChild(problemEl);
      }
    } else {
      problemsDiv.hidden = true;
    }

    // driver bug workarounds list
    if (featureInfo.workarounds.length) {
      workaroundsDiv.hidden = false;
      workaroundsList.textContent = '';
      for (const workaround of featureInfo.workarounds) {
        const workaroundEl = document.createElement('li');
        workaroundEl.textContent = workaround;
        workaroundsList.appendChild(workaroundEl);
      }
    } else {
      workaroundsDiv.hidden = true;
    }
  }

  private createProblemEl_(problem: Problem): HTMLElement {
    const problemEl = document.createElement('li');

    // Description of issue
    const desc = document.createElement('a');
    let text = problem.description;
    const pattern = ' Please update your graphics driver via this link: ';
    const pos = text.search(pattern);
    let url = '';
    if (pos > 0) {
      url = text.substring(pos + pattern.length);
      text = text.substring(0, pos);
    }
    desc.textContent = text;
    problemEl.appendChild(desc);

    // Spacing ':' element
    if (problem.crBugs.length > 0) {
      const tmp = document.createElement('span');
      tmp.textContent = ': ';
      problemEl.appendChild(tmp);
    }

    let nbugs = 0;
    let j;

    // crBugs
    for (j = 0; j < problem.crBugs.length; ++j) {
      if (nbugs > 0) {
        const tmp = document.createElement('span');
        tmp.textContent = ', ';
        problemEl.appendChild(tmp);
      }

      const link = document.createElement('a');
      const bugid = parseInt(problem.crBugs[j]!);
      link.textContent = bugid.toString();
      link.href = 'http://crbug.com/' + bugid;
      problemEl.appendChild(link);
      nbugs++;
    }

    if (problem.affectedGpuSettings.length > 0) {
      const brNode = document.createElement('br');
      problemEl.appendChild(brNode);

      const iNode = document.createElement('i');
      problemEl.appendChild(iNode);

      const headNode = document.createElement('span');
      if (problem.tag === 'disabledFeatures') {
        headNode.textContent = 'Disabled Features: ';
      } else {  // problem.tag === 'workarounds'
        headNode.textContent = 'Applied Workarounds: ';
      }
      iNode.appendChild(headNode);
      for (j = 0; j < problem.affectedGpuSettings.length; ++j) {
        if (j > 0) {
          const separateNode = document.createElement('span');
          separateNode.textContent = ', ';
          iNode.appendChild(separateNode);
        }
        const nameNode = document.createElement('span');
        if (problem.tag === 'disabledFeatures') {
          nameNode.classList.add('feature-red');
        } else {  // problem.tag === 'workarounds'
          nameNode.classList.add('feature-yellow');
        }
        nameNode.textContent = problem.affectedGpuSettings[j]!;
        iNode.appendChild(nameNode);
      }
    }

    // Append driver update link.
    if (pos > 0) {
      const brNode = document.createElement('br');
      problemEl.appendChild(brNode);

      const bNode = document.createElement('b');
      bNode.classList.add('bg-yellow');
      problemEl.appendChild(bNode);

      const tmp = document.createElement('span');
      tmp.textContent = 'Please update your graphics driver via ';
      bNode.appendChild(tmp);

      const link = document.createElement('a');
      link.textContent = 'this link';
      link.href = url;
      bNode.appendChild(link);
    }

    return problemEl;
  }

  private createAngleFeatureEl_(angleFeature: AngleFeature) {
    const angleFeatureEl = document.createElement('li');

    // Name comes first, bolded
    const name = document.createElement('b');
    name.textContent = angleFeature.name;
    angleFeatureEl.appendChild(name);

    // If there's a category, it follows the name in parentheses
    if (angleFeature.category) {
      const separator = document.createElement('span');
      separator.textContent = ' ';
      angleFeatureEl.appendChild(separator);

      const category = document.createElement('span');
      category.textContent = '(' + angleFeature.category + ')';
      angleFeatureEl.appendChild(category);
    }

    // If there's a bug link, try to parse the crbug/anglebug number
    if (angleFeature.bug) {
      const separator = document.createElement('span');
      separator.textContent = ' ';
      angleFeatureEl.appendChild(separator);

      const bug = document.createElement('a');
      if (angleFeature.bug.includes('crbug.com/')) {
        bug.textContent = angleFeature.bug.match(/\d+/)!.toString();
      } else if (angleFeature.bug.includes('anglebug.com/')) {
        bug.textContent = 'anglebug:' + angleFeature.bug.match(/\d+/);
      } else {
        bug.textContent = angleFeature.bug;
      }
      bug.href = angleFeature.bug;
      angleFeatureEl.appendChild(bug);
    }

    // Follow with a colon, and the status (colored)
    const separator = document.createElement('span');
    separator.textContent = ': ';
    angleFeatureEl.appendChild(separator);

    const status = document.createElement('span');
    if (angleFeature.status === 'enabled') {
      status.textContent = 'Enabled';
      status.classList.add('feature-green');
    } else {
      status.textContent = 'Disabled';
      status.classList.add('feature-red');
    }
    angleFeatureEl.appendChild(status);

    if (angleFeature.condition) {
      const condition = document.createElement('span');
      condition.textContent = ': ' + angleFeature.condition;
      condition.classList.add('feature-gray');
      angleFeatureEl.appendChild(condition);
    }

    // if there's a description, put on new line, italicized
    if (angleFeature.description) {
      const brNode = document.createElement('br');
      angleFeatureEl.appendChild(brNode);

      const iNode = document.createElement('i');
      angleFeatureEl.appendChild(iNode);

      const description = document.createElement('span');
      description.textContent = angleFeature.description;
      iNode.appendChild(description);
    }

    return angleFeatureEl;
  }

  private setText_(outputElementId: string, text: string) {
    this.getRequiredElement(`#${outputElementId}`).textContent = text;
  }

  private setTable_(outputElementId: string, inputData: Data[]|ArrayData[]) {
    const table = document.createElement('info-view-table');
    table.setData(inputData);

    const peg = this.$(`#${outputElementId}`);
    if (!peg) {
      throw new Error('Node ' + outputElementId + ' not found');
    }

    peg.innerHTML = window.trustedTypes!.emptyHTML;
    peg.appendChild(table);
  }

  private createDawnInfoEl_(dawnInfoList: HTMLElement, gpuDawnInfo: string[]) {
    dawnInfoList.textContent = '';
    let inProcessingToggles = false;

    for (let i = 0; i < gpuDawnInfo.length; ++i) {
      let infoString = gpuDawnInfo[i]!;
      let infoEl: HTMLElement;

      if (infoString.startsWith('<')) {
        // GPU type and backend type.
        // Add an empty line for the next adaptor.
        const separator = document.createElement('br');
        separator.textContent = '';
        dawnInfoList.appendChild(separator);

        // e.g. <Discrete GPU> D3D12 backend
        infoEl = document.createElement('h3');
        infoEl.textContent = infoString;
        dawnInfoList.appendChild(infoEl);
        inProcessingToggles = false;
      } else if (infoString.startsWith('[')) {
        // e.g. [Default Toggle Names]
        infoEl = document.createElement('h4');
        infoEl.classList.add('dawn-info-header');
        infoEl.textContent = infoString;

        if (infoString === '[WebGPU Status]' ||
            infoString === '[Default Supported Features]') {
          inProcessingToggles = false;
        } else {
          inProcessingToggles = true;
        }
      } else if (inProcessingToggles) {
        // Each toggle takes 3 strings
        infoEl = document.createElement('li');

        // The toggle name comes first, bolded.
        const name = document.createElement('b');
        name.textContent = infoString + ':  ';
        infoEl.appendChild(name);

        // URL
        infoString = gpuDawnInfo[++i]!;
        const url = document.createElement('a');
        url.textContent = infoString;
        url.href = infoString;
        infoEl.appendChild(url);

        // Description, italicized
        infoString = gpuDawnInfo[++i]!;
        const description = document.createElement('i');
        description.textContent = ':  ' + infoString;
        infoEl.appendChild(description);
      } else {
        // Display supported extensions
        infoEl = document.createElement('li');
        infoEl.textContent = infoString;
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
