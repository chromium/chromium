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
   * @extends {cr.ui.TabPanel}
   */
  var InfoView = cr.ui.define(cr.ui.TabPanel);

  InfoView.prototype = {
    __proto__: cr.ui.TabPanel.prototype,

    decorate: function() {
      cr.ui.TabPanel.prototype.decorate.apply(this);

      browserBridge.addEventListener('gpuInfoUpdate', this.refresh.bind(this));
      browserBridge.addEventListener(
          'logMessagesChange', this.refresh.bind(this));
      browserBridge.addEventListener(
          'clientInfoChange', this.refresh.bind(this));

      // Add handler to 'copy to clipboard' button
      document.getElementById('copy-to-clipboard').onclick =
          function() {
        // Make sure nothing is selected
        window.getSelection().removeAllRanges();

        document.execCommand('selectAll');
        document.execCommand('copy');

        // And deselect everything at the end.
        window.getSelection().removeAllRanges();
      }

          this.refresh();
    },

    /**
     * Updates the view based on its currently known data
     */
    refresh: function(data) {
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
        var clientInfo = browserBridge.clientInfo;

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
      var diagnosticsDiv = this.querySelector('.diagnostics');
      var diagnosticsLoadingDiv = this.querySelector('.diagnostics-loading');
      var featureStatusList = this.querySelector('.feature-status-list');
      var problemsDiv = this.querySelector('.problems-div');
      var problemsList = this.querySelector('.problems-list');
      var workaroundsDiv = this.querySelector('.workarounds-div');
      var workaroundsList = this.querySelector('.workarounds-list');

      var basicInfoForHardwareGpuDiv =
          this.querySelector('.basic-info-for-hardware-gpu-div');
      var featureStatusForHardwareGpuDiv =
          this.querySelector('.feature-status-for-hardware-gpu-div');
      var featureStatusForHardwareGpuList =
          this.querySelector('.feature-status-for-hardware-gpu-list');
      var problemsForHardwareGpuDiv =
          this.querySelector('.problems-for-hardware-gpu-div');
      var problemsForHardwareGpuList =
          this.querySelector('.problems-for-hardware-gpu-list');
      var workaroundsForHardwareGpuDiv =
          this.querySelector('.workarounds-for-hardware-gpu-div');
      var workaroundsForHardwareGpuList =
          this.querySelector('.workarounds-for-hardware-gpu-list');

      var gpuInfo = browserBridge.gpuInfo;
      var i;
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

        if (gpuInfo.basicInfo)
          this.setTable_('basic-info', gpuInfo.basicInfo);
        else
          this.setTable_('basic-info', []);

        if (gpuInfo.compositorInfo)
          this.setTable_('compositor-info', gpuInfo.compositorInfo);
        else
          this.setTable_('compositor-info', []);

        if (gpuInfo.gpuMemoryBufferInfo)
          this.setTable_('gpu-memory-buffer-info', gpuInfo.gpuMemoryBufferInfo);
        else
          this.setTable_('gpu-memory-buffer-info', []);

        if (gpuInfo.displayInfo)
          this.setTable_('display-info', gpuInfo.displayInfo);
        else
          this.setTable_('display-info', []);

        if (gpuInfo.videoAcceleratorsInfo) {
          this.setTable_(
              'video-acceleration-info', gpuInfo.videoAcceleratorsInfo);
        } else {
          this.setTable_('video-acceleration-info', []);
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
      var featureLabelMap = {
        '2d_canvas': 'Canvas',
        'gpu_compositing': 'Compositing',
        'webgl': 'WebGL',
        'multisampling': 'WebGL multisampling',
        'flash_3d': 'Flash',
        'flash_stage3d': 'Flash Stage3D',
        'flash_stage3d_baseline': 'Flash Stage3D Baseline profile',
        'texture_sharing': 'Texture Sharing',
        'video_decode': 'Video Decode',
        'rasterization': 'Rasterization',
        'oop_rasterization': 'Out-of-process Rasterization',
        'multiple_raster_threads': 'Multiple Raster Threads',
        'native_gpu_memory_buffers': 'Native GpuMemoryBuffers',
        'protected_video_decode': 'Hardware Protected Video Decode',
        'surface_synchronization': 'Surface Synchronization',
        'surface_control': 'Surface Control',
        'vpx_decode': 'VPx Video Decode',
        'webgl2': 'WebGL2',
        'viz_display_compositor': 'Viz Service Display Compositor',
        'skia_renderer': 'Skia Renderer',
        'skia_deferred_display_list': 'Skia Deferred Display List',
      };

      var statusMap = {
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
      for (var featureName in featureInfo.featureStatus) {
        var featureStatus = featureInfo.featureStatus[featureName];
        var featureEl = document.createElement('li');

        var nameEl = document.createElement('span');
        if (!featureLabelMap[featureName])
          console.log('Missing featureLabel for', featureName);
        nameEl.textContent = featureLabelMap[featureName] + ': ';
        featureEl.appendChild(nameEl);

        var statusEl = document.createElement('span');
        var statusInfo = statusMap[featureStatus];
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
          var problem = featureInfo.problems[i];
          var problemEl = this.createProblemEl_(problem);
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
          var workaroundEl = document.createElement('li');
          workaroundEl.textContent = featureInfo.workarounds[i];
          workaroundsList.appendChild(workaroundEl);
        }
      } else {
        workaroundsDiv.hidden = true;
      }
    },

    createProblemEl_: function(problem) {
      var problemEl;
      problemEl = document.createElement('li');

      // Description of issue
      var desc = document.createElement('a');
      desc.textContent = problem.description;
      problemEl.appendChild(desc);

      // Spacing ':' element
      if (problem.crBugs.length > 0) {
        var tmp = document.createElement('span');
        tmp.textContent = ': ';
        problemEl.appendChild(tmp);
      }

      var nbugs = 0;
      var j;

      // crBugs
      for (j = 0; j < problem.crBugs.length; ++j) {
        if (nbugs > 0) {
          var tmp = document.createElement('span');
          tmp.textContent = ', ';
          problemEl.appendChild(tmp);
        }

        var link = document.createElement('a');
        var bugid = parseInt(problem.crBugs[j]);
        link.textContent = bugid;
        link.href = 'http://crbug.com/' + bugid;
        problemEl.appendChild(link);
        nbugs++;
      }

      if (problem.affectedGpuSettings.length > 0) {
        var brNode = document.createElement('br');
        problemEl.appendChild(brNode);

        var iNode = document.createElement('i');
        problemEl.appendChild(iNode);

        var headNode = document.createElement('span');
        if (problem.tag == 'disabledFeatures')
          headNode.textContent = 'Disabled Features: ';
        else  // problem.tag == 'workarounds'
          headNode.textContent = 'Applied Workarounds: ';
        iNode.appendChild(headNode);
        for (j = 0; j < problem.affectedGpuSettings.length; ++j) {
          if (j > 0) {
            var separateNode = document.createElement('span');
            separateNode.textContent = ', ';
            iNode.appendChild(separateNode);
          }
          var nameNode = document.createElement('span');
          if (problem.tag == 'disabledFeatures')
            nameNode.classList.add('feature-red');
          else  // problem.tag == 'workarounds'
            nameNode.classList.add('feature-yellow');
          nameNode.textContent = problem.affectedGpuSettings[j];
          iNode.appendChild(nameNode);
        }
      }

      return problemEl;
    },

    setText_: function(outputElementId, text) {
      var peg = document.getElementById(outputElementId);
      peg.textContent = text;
    },

    setTable_: function(outputElementId, inputData) {
      var template = jstGetTemplate('info-view-table-template');
      jstProcess(new JsEvalContext({value: inputData}), template);

      var peg = document.getElementById(outputElementId);
      if (!peg)
        throw new Error('Node ' + outputElementId + ' not found');

      peg.innerHTML = '';
      peg.appendChild(template);
    }
  };

  return {InfoView: InfoView};
});
