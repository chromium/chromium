// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var container, stats;
var camera, controls, scene, projector, renderer;
var plane;
var lastSceneDescription;
var skipSceneUpdates = 0;
var hold = false;
var holdObjectIndex = -1;

var mouse = new THREE.Vector2();
var offset = new THREE.Vector3();
var INTERSECTED, SELECTED;

var sceneDescription = [];

var shapes = {};
var objects = [];

function clearWorld() {
  for (var i = 0; i < objects.length; i++) {
    scene.remove(objects[i]);
  }
  objects = [];
  shapes = {};
  // Make sure we drop the object.
  hold = false;
  SELECTED = undefined;
  NaClAMBulletDropObject();
}

function loadShape(shape) {
  if (shapes[shape.name] != undefined) {
    return shapes[shape.name];
  }

  if (shape.type == "cube") {
    shapes[shape.name] = new THREE.CubeGeometry(shape['wx'], shape['wy'], shape['wz']);
    return shapes[shape.name];
  }

  if (shape.type == "convex") {
    var vertices = [];
    for (var i = 0; i < shape['points'].length; i++) {
      vertices.push(new THREE.Vector3(shape['points'][i][0], shape['points'][i][1], shape['points'][i][2]));
    }
    shapes[shape.name] = new THREE.ConvexGeometry(vertices);
    return shapes[shape.name];
  }

  if (shape.type == "cylinder") {
    shapes[shape.name] = new THREE.CylinderGeometry(shape['radius'], shape['radius'], shape['height'])
      return shapes[shape.name];
  }

  if (shape.type == "sphere") {
    shapes[shape.name] = new THREE.SphereGeometry(shape['radius']);
    return shapes[shape.name];
  }

  return undefined;
}

function loadBody(body) {
  var shape = shapes[body.shape];
  if (shape == undefined) {
    return shape;
  }

  var object = new THREE.Mesh( shape, new THREE.MeshLambertMaterial( { color: Math.random() * 0xffffff } ) );

  object.material.ambient = object.material.color;

  object.position.x = body.position.x;
  object.position.y = body.position.y;
  object.position.z = body.position.z;

  object.rotation.x = body.rotation.x;
  object.rotation.y = body.rotation.y;
  object.rotation.z = body.rotation.z;

  object.updateMatrixWorld(true);
  var T = [object.matrixWorld.elements[0],
      object.matrixWorld.elements[1],
      object.matrixWorld.elements[2],
      object.matrixWorld.elements[3],
      object.matrixWorld.elements[4],
      object.matrixWorld.elements[5],
      object.matrixWorld.elements[6],
      object.matrixWorld.elements[7],
      object.matrixWorld.elements[8],
      object.matrixWorld.elements[9],
      object.matrixWorld.elements[10],
      object.matrixWorld.elements[11],
      object.matrixWorld.elements[12],
      object.matrixWorld.elements[13],
      object.matrixWorld.elements[14],
      object.matrixWorld.elements[15]];
  body.transform = T;

  object.castShadow = false;
  object.receiveShadow = false;
  object.matrixAutoUpdate = false;
  object.objectTableIndex = objects.length;
  scene.add(object);
  objects.push(object);

  return object;
}

function loadWorld(worldDescription) {
  clearWorld();
  var i;
  var shapes = worldDescription['shapes'];
  var bodies = worldDescription['bodies'];
  for (i = 0; i < shapes.length; i++) {
    if (loadShape(shapes[i]) == undefined) {
      console.log('Could not load shape ' + shapes[i].name);
    }
  }

  for (i = 0; i < bodies.length; i++) {
    if (loadBody(bodies[i]) == undefined) {
      console.log('Could not make body.');
    }
  }

  var r = verifyWorldDescription(worldDescription);
  if (r == false) {
    alert('Invalid scene description. See console.');
    return;
  }
  skipSceneUpdates = 4;
  NaClAMBulletLoadScene(worldDescription);
  lastSceneDescription = worldDescription;
}

function reloadScene() {
  if (lastSceneDescription)
    loadWorld(lastSceneDescription);
}

function $(id) {
  return document.getElementById(id);
}

function init() {
  var rendererContainer = $('rendererContainer');
  var rcW = rendererContainer.clientWidth;
  var rcH = rendererContainer.clientHeight;

  camera = new THREE.PerspectiveCamera(
      70,
      rcW / rcH, 1, 10000);
  camera.position.y = 20.0;
  camera.position.z = 40;

  scene = new THREE.Scene();

  scene.add( new THREE.AmbientLight( 0x505050 ) );

  var light = new THREE.SpotLight( 0xffffff, 1.5 );
  light.position.set( 0, 500, 2000 );
  light.castShadow = true;

  light.shadowCameraNear = 200;
  light.shadowCameraFar = camera.far;
  light.shadowCameraFov = 50;

  light.shadowBias = -0.00022;
  light.shadowDarkness = 0.5;

  light.shadowMapWidth = 2048;
  light.shadowMapHeight = 2048;

  scene.add( light );

  plane = new THREE.Mesh( new THREE.PlaneGeometry( 200, 200, 100, 100), new THREE.MeshBasicMaterial( { color: 0x000000, opacity: 0.25, transparent: true, wireframe: true } ) );
  plane.rotation.x = Math.PI * 0.5;
  plane.visible = true;
  scene.add( plane );
  projector = new THREE.Projector();

  renderer = new THREE.WebGLRenderer( { antialias: true } );
  renderer.sortObjects = false;
  renderer.setSize( rcW, rcH );
  lastRendererWidth = rcW;
  lastRendererWidth = rcH;

  renderer.shadowMapEnabled = true;
  renderer.shadowMapSoft = true;

  rendererContainer.appendChild(renderer.domElement);

  var idFuncHash = {
    jenga10: loadJenga10,
    jenga20: loadJenga20,
    randomShapes: loadRandomShapes,
    randomCube250: load250RandomCubes,
    randomCylinder500: load500RandomCylinders,
    randomCube1000: load1000RandomCubes,
    randomCube2000: load2000RandomCubes
  };

  for (var id in idFuncHash) {
    var func = idFuncHash[id];
    $(id).addEventListener('click', func, false);
  }

  $('reload').addEventListener('click', reloadScene, false);

  rendererContainer.addEventListener('mousedown', onMouseDown, false);
  rendererContainer.addEventListener('mouseup', onMouseUp, false);
  rendererContainer.addEventListener('mouseleave', onMouseUp, false);
  renderer.domElement.addEventListener('mousemove', onMouseMove, false);

  // Add the OrbitControls after our own listeners -- that way we can prevent
  // the camera rotation when dragging an object.
  controls = new THREE.OrbitControls(camera, rendererContainer);

  window.setInterval(pollForRendererResize, 10);
}

function pollForRendererResize() {
  var rendererContainer = $('rendererContainer');
  var w = rendererContainer.clientWidth;
  var h = rendererContainer.clientHeight;
  if (w == lastRendererWidth && h == lastRendererHeight)
    return;

  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  renderer.setSize( w, h );
  lastRendererWidth = w;
  lastRendererHeight = h;
}

function onMouseDown(event) {
  event.preventDefault();

  var vector = new THREE.Vector3( mouse.x, mouse.y, 0.5 );
  projector.unprojectVector( vector, camera );
  var ray = new THREE.Ray( camera.position, vector.subSelf( camera.position ).normalize() );
  var intersects = ray.intersectObjects( objects );
  if (intersects.length > 0) {
    if (intersects[0].object != plane) {
      hold = true;
      SELECTED = intersects[0].object;
      //console.log(SELECTED.objectTableIndex);
      NaClAMBulletPickObject(SELECTED.objectTableIndex, camera.position, intersects[0].point);
      // stopImmediatePropagation() will prevent other event listeners on the
      // same element from firing -- in this case, the OrbitControls camera
      // rotation.
      event.stopImmediatePropagation();
    }
  }
}

function onMouseUp(event) {
  if (hold) {
    hold = false;
    SELECTED = undefined;
    NaClAMBulletDropObject();
    event.stopImmediatePropagation();
  }
}

function onMouseMove( event ) {
  event.preventDefault();

  var clientRect = $('rendererContainer').getClientRects()[0];
  var x = event.clientX - clientRect.left;
  var y = event.clientY - clientRect.top;
  var w = clientRect.width;
  var h = clientRect.height;

  mouse.x = ( x / w ) * 2 - 1;
  mouse.y = -( y / h ) * 2 + 1;
  var vector = new THREE.Vector3( mouse.x, mouse.y, 0.5 );
  projector.unprojectVector( vector, camera );
  offset.x = vector.x;
  offset.y = vector.y;
  offset.z = vector.z;
}

//

function animate() {
  window.requestAnimationFrame(animate);
  aM.sendMessage('stepscene', {rayFrom: [camera.position.x, camera.position.y, camera.position.z], rayTo: [offset.x, offset.y, offset.z]});
  render();
}

function render() {
  controls.update();
  renderer.render( scene, camera );
}
