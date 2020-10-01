// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview This view displays information on the current GPU
 * hardware.  Its primary usefulness is to allow users to copy-paste
 * their data in an easy to read format for bug reports.
 */
cr.define('gpu', function() {
  /**
   * Provides information on the GPU process and underlying graphics hardware.
   * @constructor
   */
  const InfoView = cr.ui.define('div');

  InfoView.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      browserBridge.addEventListener('gpuInfoUpdate', this.refresh.bind(this));
      browserBridge.addEventListener(
          'logMessagesChange', this.refresh.bind(this));
      browserBridge.addEventListener(
          'clientInfoChange', this.refresh.bind(this));

      // Add handler to 'copy to clipboard' button
      $('copy-to-clipboard').onclick = function() {
        // Make sure nothing is selected
        window.getSelection().removeAllRanges();

        document.execCommand('selectAll');
        document.execCommand('copy');

        // And deselect everything at the end.
        window.getSelection().removeAllRanges();
      };

      this.refresh();
    },

    /**
     * Updates the view based on its currently known data
     */
    refresh: function(data) {
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
                'gpu/config/software_rendering_list.json')
          },
          {
            description: 'Driver bug list URL',
            value: createSourcePermalink(
                clientInfo.revision_identifier,
                'gpu/config/gpu_driver_bug_list.json')
          },
          {description: 'ANGLE commit id', value: clientInfo.angle_commit_id}, {
            description: '2D graphics backend',
            value: clientInfo.graphics_backend
          },
          {description: 'Command Line', value: clientInfo.command_line}
        ]);
      } else {
        this.setText_('client-info', '... loading...');
      }


      // GPU info, basic
      const diagnosticsDiv = this.querySelector('.diagnostics');
      const diagnosticsLoadingDiv = this.querySelector('.diagnostics-loading');
      const featureStatusList = this.querySelector('.feature-status-list');
      const problemsDiv = this.querySelector('.problems-div');
      const problemsList = this.querySelector('.problems-list');
      const workaroundsDiv = this.querySelector('.workarounds-div');
      const workaroundsList = this.querySelector('.workarounds-list');
      const ANGLEFeaturesDiv = this.querySelector('.angle-features-div');
      const ANGLEFeaturesList = this.querySelector('.angle-features-list');

      const basicInfoForHardwareGpuDiv =
          this.querySelector('.basic-info-for-hardware-gpu-div');
      const featureStatusForHardwareGpuDiv =
          this.querySelector('.feature-status-for-hardware-gpu-div');
      const featureStatusForHardwareGpuList =
          this.querySelector('.feature-status-for-hardware-gpu-list');
      const problemsForHardwareGpuDiv =
          this.querySelector('.problems-for-hardware-gpu-div');
      const problemsForHardwareGpuList =
          this.querySelector('.problems-for-hardware-gpu-list');
      const workaroundsForHardwareGpuDiv =
          this.querySelector('.workarounds-for-hardware-gpu-div');
      const workaroundsForHardwareGpuList =
          this.querySelector('.workarounds-for-hardware-gpu-list');

      const gpuInfo = browserBridge.gpuInfo;
      let i;
      if (gpuInfo) {
        // Not using jstemplate here for blacklist status because we construct
        // href from data, which jstemplate can't seem to do.
        if (gpuInfo.featureStatus) {
          this.appendFeatureInfo_(
              gpuInfo.featureStatus, featureStatusList, problemsDiv,
              problemsList, workaroundsDiv, workaroundsList);
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
            for (i = 0; i < gpuInfo.ANGLEFeatures.length; i++) {
              const ANGLEFeature = gpuInfo.ANGLEFeatures[i];
              const ANGLEFeatureEl = this.createANGLEFeatureEl_(ANGLEFeature);
              ANGLEFeaturesList.appendChild(ANGLEFeatureEl);
            }
          } else {
            ANGLEFeaturesDiv.hidden = true;
          }
        }

        if (gpuInfo.diagnostics) {
          diagnosticsDiv.hidden = false;
          diagnosticsLoadingDiv.hidden = true;
          $('diagnostics-table').hidden = false;
          this.setTable_('diagnostics-table', gpuInfo.diagnostics);
        } else if (gpuInfo.diagnostics === null) {
          // gpu_internals.cc sets diagnostics to null when it is being loaded
          diagnosticsDiv.hidden = false;
          diagnosticsLoadingDiv.hidden = false;
          $('diagnostics-table').hidden = true;
        } else {
          diagnosticsDiv.hidden = true;
        }

        if (gpuInfo.vulkanInfo) {
          const vulkanInfo = new gpu.VulkanInfo(gpuInfo.vulkanInfo);
          const data = [{
            'description': 'info',
            'value': vulkanInfo.toString(),
            'id': 'vulkan-info-value'
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
      }

      // Log messages
      jstProcess(
          new JsEvalContext({values: browserBridge.logMessages}),
          $('log-messages'));
    },

    appendFeatureInfo_: function(
        featureInfo, featureStatusList, problemsDiv, problemsList,
        workaroundsDiv, workaroundsList) {
      // Feature map
      const featureLabelMap = {
        '2d_canvas': 'Canvas',
        'gpu_compositing': 'Compositing',
        'webgl': 'WebGL',
        'multisampling': 'WebGL multisampling',
        'texture_sharing': 'Texture Sharing',
        'video_decode': 'Video Decode',
        'rasterization': 'Rasterization',
        'oop_rasterization': 'Out-of-process Rasterization',
        'opengl': 'OpenGL',
        'metal': 'Metal',
        'vulkan': 'Vulkan',
        'multiple_raster_threads': 'Multiple Raster Threads',
        'native_gpu_memory_buffers': 'Native GpuMemoryBuffers',
        'protected_video_decode': 'Hardware Protected Video Decode',
        'surface_control': 'Surface Control',
        'vpx_decode': 'VPx Video Decode',
        'webgl2': 'WebGL2',
        'skia_renderer': 'Skia Renderer',
      };

      const statusMap = {
        'disabled_software': {
          'label': 'Software only. Hardware acceleration disabled',
          'class': 'feature-yellow'
        },
        'disabled_off': {'label': 'Disabled', 'class': 'feature-red'},
        'disabled_off_ok': {'label': 'Disabled', 'class': 'feature-yellow'},
        'unavailable_software': {
          'label': 'Software only, hardware acceleration unavailable',
          'class': 'feature-yellow'
        },
        'unavailable_off': {'label': 'Unavailable', 'class': 'feature-red'},
        'unavailable_off_ok':
            {'label': 'Unavailable', 'class': 'feature-yellow'},
        'enabled_readback': {
          'label': 'Hardware accelerated but at reduced performance',
          'class': 'feature-yellow'
        },
        'enabled_force': {
          'label': 'Hardware accelerated on all pages',
          'class': 'feature-green'
        },
        'enabled': {'label': 'Hardware accelerated', 'class': 'feature-green'},
        'enabled_on': {'label': 'Enabled', 'class': 'feature-green'},
        'enabled_force_on':
            {'label': 'Force enabled', 'class': 'feature-green'},
      };

      // feature status list
      featureStatusList.textContent = '';
      for (const featureName in featureInfo.featureStatus) {
        const featureStatus = featureInfo.featureStatus[featureName];
        const featureEl = document.createElement('li');

        const nameEl = document.createElement('span');
        if (!featureLabelMap[featureName]) {
          console.log('Missing featureLabel for', featureName);
        }
        nameEl.textContent = featureLabelMap[featureName] + ': ';
        featureEl.appendChild(nameEl);

        const statusEl = document.createElement('span');
        const statusInfo = statusMap[featureStatus];
        if (!statusInfo) {
          console.log('Missing status for ', featureStatus);
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
        for (i = 0; i < featureInfo.problems.length; i++) {
          const problem = featureInfo.problems[i];
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
        for (i = 0; i < featureInfo.workarounds.length; i++) {
          const workaroundEl = document.createElement('li');
          workaroundEl.textContent = featureInfo.workarounds[i];
          workaroundsList.appendChild(workaroundEl);
        }
      } else {
        workaroundsDiv.hidden = true;
      }
    },

    createProblemEl_: function(problem) {
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
    },

    createANGLEFeatureEl_: function(ANGLEFeature) {
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
    },

    setText_: function(outputElementId, text) {
      const peg = $(outputElementId);
      peg.textContent = text;
    },

    setTable_: function(outputElementId, inputData) {
      const template = jstGetTemplate('info-view-table-template');
      jstProcess(new JsEvalContext({value: inputData}), template);

      const peg = $(outputElementId);
      if (!peg) {
        throw new Error('Node ' + outputElementId + ' not found');
      }

      peg.innerHTML = trustedTypes.emptyHTML;
      peg.appendChild(template);
    }
  };

  return {InfoView: InfoView};
});
