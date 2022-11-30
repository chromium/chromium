// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function NaClAMBulletInit() {
  aM.addEventListener('sceneloaded', NaClAMBulletSceneLoadedHandler);
  aM.addEventListener('noscene', NaClAMBulletStepSceneHandler);
  aM.addEventListener('sceneupdate', NaClAMBulletStepSceneHandler);
}

function NaClAMBulletLoadScene(sceneDescription) {
  aM.sendMessage('loadscene', sceneDescription);
}

function NaClAMBulletSceneLoadedHandler(msg) {
  console.log('Scene loaded.');
  console.log('Scene object count = ' + msg.header.sceneobjectcount);
}

function NaClAMBulletPickObject(objectTableIndex, cameraPos, hitPos) {
  aM.sendMessage('pickobject', {index: objectTableIndex, cpos: [cameraPos.x, cameraPos.y, cameraPos.z], pos: [hitPos.x,hitPos.y,hitPos.z]});
}

function NaClAMBulletDropObject() {
  aM.sendMessage('dropobject', {});
}

// Values used to display simulation time every second.
var fps = {
  lastTimeMs: +new Date(),
  sumSimTime: 0,
  numSteps: 0
};

function NaClAMBulletStepSceneHandler(msg) {
  // Step the scene
  var i;
  var j;
  var numTransforms = 0;
  if (msg.header.cmd == 'sceneupdate') {
    if (skipSceneUpdates > 0) {
      skipSceneUpdates--;
      return;
    }
    TransformBuffer = new Float32Array(msg.frames[0]);
    numTransforms = TransformBuffer.length/16;
    for (i = 0; i < numTransforms; i++) {
      for (j = 0; j < 16; j++) {
        objects[i].matrixWorld.elements[j] = TransformBuffer[i*16+j];
      }
    }

    var simTime = msg.header.simtime;
    fps.sumSimTime += simTime;
    fps.numSteps++;

    // Update FPS.
    var curTimeMs = +new Date();
    if (curTimeMs - fps.lastTimeMs > 1000) {  // 1 sec
      var meanSimTime = fps.sumSimTime / fps.numSteps;
      $('simulationTime').textContent = meanSimTime.toFixed(0);
      $('fps').textContent =
          (fps.numSteps * 1000 / (curTimeMs - fps.lastTimeMs)).toFixed(1);
      fps.lastTimeMs = curTimeMs;
      fps.sumSimTime = 0;
      fps.numSteps = 0;
    }
  }
}
