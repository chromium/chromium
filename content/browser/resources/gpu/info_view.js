// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_view_table.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './info_view.html.js';
import {VulkanInfo} from './vulkan_info.js';

/**
 * @fileoverview This view displays information on the current GPU
 * hardware.  Its primary usefulness is to allow users to copy-paste
 * their data in an easy to read format for bug reports.
 */
export class InfoViewElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  addBrowserBridgeListeners(browserBridge) {
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
    this.shadowRoot.querySelector('#copy-to-clipboard').onclick = (() => {
      // Make sure nothing is selected
      const s = window.getSelection();
      s.removeAllRanges();
      s.selectAllChildren(this.shadowRoot);
      document.execCommand('copy');

      // And deselect everything at the end.
      window.getSelection().removeAllRanges();
    });
  }

  /**
   * Updates the view based on its currently known data
   */
  refresh(browserBridge) {
    let clientInfo;
    function createSourcePermalink(revisionIdentifier, filepath) {
      if (revisionIdentifier.length !== 40) {
        // If the revision id isn't a hash, just use the 0.0.0.0 version
        // from the Chrome version string "Chrome/0.0.0.0".
        revisionIdentifier = clientInfo.version.split('/')[1];
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
    const diagnosticsDiv = this.shadowRoot.querySelector('.diagnostics');
    const diagnosticsLoadingDiv =
        this.shadowRoot.querySelector('.diagnostics-loading');
    const featureStatusList =
        this.shadowRoot.querySelector('.feature-status-list');
    const problemsDiv = this.shadowRoot.querySelector('.problems-div');
    const problemsList = this.shadowRoot.querySelector('.problems-list');
    const workaroundsDiv = this.shadowRoot.querySelector('.workarounds-div');
    const workaroundsList = this.shadowRoot.querySelector('.workarounds-list');
    const ANGLEFeaturesDiv =
        this.shadowRoot.querySelector('.angle-features-div');
    const ANGLEFeaturesList =
        this.shadowRoot.querySelector('.angle-features-list');
    const DAWNInfoDiv = this.shadowRoot.querySelector('.dawn-info-div');
    const DAWNInfoList = this.shadowRoot.querySelector('.dawn-info-list');

    const basicInfoForHardwareGpuDiv =
        this.shadowRoot.querySelector('.basic-info-for-hardware-gpu-div');
    const featureStatusForHardwareGpuDiv =
        this.shadowRoot.querySelector('.feature-status-for-hardware-gpu-div');
    const featureStatusForHardwareGpuList =
        this.shadowRoot.querySelector('.feature-status-for-hardware-gpu-list');
    const problemsForHardwareGpuDiv =
        this.shadowRoot.querySelector('.problems-for-hardware-gpu-div');
    const problemsForHardwareGpuList =
        this.shadowRoot.querySelector('.problems-for-hardware-gpu-list');
    const workaroundsForHardwareGpuDiv =
        this.shadowRoot.querySelector('.workarounds-for-hardware-gpu-div');
    const workaroundsForHardwareGpuList =
        this.shadowRoot.querySelector('.workarounds-for-hardware-gpu-list');

    const gpuInfo = browserBridge.gpuInfo;
    let i;
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
          ANGLEFeaturesDiv.hidden = false;
          ANGLEFeaturesList.textContent = '';
          for (const ANGLEFeature of gpuInfo.ANGLEFeatures) {
            const ANGLEFeatureEl = this.createANGLEFeatureEl_(ANGLEFeature);
            ANGLEFeaturesList.appendChild(ANGLEFeatureEl);
          }
        } else {
          ANGLEFeaturesDiv.hidden = true;
        }
      }

      if (gpuInfo.dawnInfo) {
        if (gpuInfo.dawnInfo.length) {
          DAWNInfoDiv.hidden = false;
          this.createDawnInfoEl_(DAWNInfoList, gpuInfo.dawnInfo);
        } else {
          DAWNInfoDiv.hidden = true;
        }
      }

      if (gpuInfo.diagnostics) {
        diagnosticsDiv.hidden = false;
        diagnosticsLoadingDiv.hidden = true;
        this.shadowRoot.querySelector('#diagnostics-table').hidden = false;
        this.setTable_('diagnostics-table', gpuInfo.diagnostics);
      } else if (gpuInfo.diagnostics === null) {
        // gpu_internals.cc sets diagnostics to null when it is being loaded
        diagnosticsDiv.hidden = false;
        diagnosticsLoadingDiv.hidden = false;
        this.shadowRoot.querySelector('#diagnostics-table').hidden = true;
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
      DAWNInfoDiv.hidden = true;
    }

    // Log messages
    const messageList = this.shadowRoot.querySelector('#log-messages > ul');
    messageList.innerHTML =
        window.trustedTypes ? window.trustedTypes.emptyHTML : '';
    browserBridge.logMessages.forEach(messageObj => {
      const messageEl = document.createElement('span');
      messageEl.textContent = `${messageObj.header}: ${messageObj.message}`;
      const li = document.createElement('li');
      li.appendChild(messageEl);
      messageList.appendChild(li);
    });
  }

  appendFeatureInfo_(
      featureInfo, featureStatusList, problemsDiv, problemsList, workaroundsDiv,
      workaroundsList) {
    // Feature map
    const featureLabelMap = {
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
    };

    const statusMap = {
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
      const featureStatus = featureInfo.featureStatus[featureName];
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

  createProblemEl_(problem) {
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
      const bugid = parseInt(problem.crBugs[j]);
      link.textContent = bugid;
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
        nameNode.textContent = problem.affectedGpuSettings[j];
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

  createANGLEFeatureEl_(ANGLEFeature) {
    const ANGLEFeatureEl = document.createElement('li');

    // Name comes first, bolded
    const name = document.createElement('b');
    name.textContent = ANGLEFeature.name;
    ANGLEFeatureEl.appendChild(name);

    // If there's a category, it follows the name in parentheses
    if (ANGLEFeature.category) {
      const separator = document.createElement('span');
      separator.textContent = ' ';
      ANGLEFeatureEl.appendChild(separator);

      const category = document.createElement('span');
      category.textContent = '(' + ANGLEFeature.category + ')';
      ANGLEFeatureEl.appendChild(category);
    }

    // If there's a bug link, try to parse the crbug/anglebug number
    if (ANGLEFeature.bug) {
      const separator = document.createElement('span');
      separator.textContent = ' ';
      ANGLEFeatureEl.appendChild(separator);

      const bug = document.createElement('a');
      if (ANGLEFeature.bug.includes('crbug.com/')) {
        bug.textContent = ANGLEFeature.bug.match(/\d+/);
      } else if (ANGLEFeature.bug.includes('anglebug.com/')) {
        bug.textContent = 'anglebug:' + ANGLEFeature.bug.match(/\d+/);
      } else {
        bug.textContent = ANGLEFeature.bug;
      }
      bug.href = ANGLEFeature.bug;
      ANGLEFeatureEl.appendChild(bug);
    }

    // Follow with a colon, and the status (colored)
    const separator = document.createElement('span');
    separator.textContent = ': ';
    ANGLEFeatureEl.appendChild(separator);

    const status = document.createElement('span');
    if (ANGLEFeature.status === 'enabled') {
      status.textContent = 'Enabled';
      status.classList.add('feature-green');
    } else {
      status.textContent = 'Disabled';
      status.classList.add('feature-red');
    }
    ANGLEFeatureEl.appendChild(status);

    if (ANGLEFeature.condition) {
      const condition = document.createElement('span');
      condition.textContent = ': ' + ANGLEFeature.condition;
      condition.classList.add('feature-gray');
      ANGLEFeatureEl.appendChild(condition);
    }

    // if there's a description, put on new line, italicized
    if (ANGLEFeature.description) {
      const brNode = document.createElement('br');
      ANGLEFeatureEl.appendChild(brNode);

      const iNode = document.createElement('i');
      ANGLEFeatureEl.appendChild(iNode);

      const description = document.createElement('span');
      description.textContent = ANGLEFeature.description;
      iNode.appendChild(description);
    }

    return ANGLEFeatureEl;
  }

  setText_(outputElementId, text) {
    const peg = this.shadowRoot.querySelector(`#${outputElementId}`);
    peg.textContent = text;
  }

  setTable_(outputElementId, inputData) {
    const table = document.createElement('info-view-table');
    table.setData(inputData);

    const peg = this.shadowRoot.querySelector(`#${outputElementId}`);
    if (!peg) {
      throw new Error('Node ' + outputElementId + ' not found');
    }

    peg.innerHTML = window.trustedTypes.emptyHTML;
    peg.appendChild(table);
  }

  createDawnInfoEl_(DAWNInfoList, gpuDawnInfo) {
    DAWNInfoList.textContent = '';
    let inProcessingToggles = false;

    for (let i = 0; i < gpuDawnInfo.length; ++i) {
      let infoString = gpuDawnInfo[i];
      let infoEl;

      if (infoString.startsWith('<')) {
        // GPU type and backend type.
        // Add an empty line for the next adaptor.
        const separator = document.createElement('br');
        separator.textContent = '';
        DAWNInfoList.appendChild(separator);

        // e.g. <Discrete GPU> D3D12 backend
        infoEl = document.createElement('h3');
        infoEl.textContent = infoString;
        DAWNInfoList.appendChild(infoEl);
        inProcessingToggles = false;
      } else if (infoString.startsWith('[')) {
        // e.g. [Default Toggle Names]
        infoEl = document.createElement('h4');
        infoEl.classList.add('dawn-info-header');
        infoEl.textContent = infoString;

        if (infoString === '[WebGPU Status]' ||
            infoString === '[Supported Features]') {
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
        infoString = gpuDawnInfo[++i];
        const url = document.createElement('a');
        url.textContent = infoString;
        url.href = infoString;
        infoEl.appendChild(url);

        // Description, italicized
        infoString = gpuDawnInfo[++i];
        const description = document.createElement('i');
        description.textContent = ':  ' + infoString;
        infoEl.appendChild(description);
      } else {
        // Display supported extensions
        infoEl = document.createElement('li');
        infoEl.textContent = infoString;
      }

      DAWNInfoList.appendChild(infoEl);
    }
  }
}

customElements.define('info-view', InfoViewElement);
