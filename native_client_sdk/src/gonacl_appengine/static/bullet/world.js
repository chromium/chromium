// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function verifyWorldDescription(description) {
  var shapes = description['shapes'];
  var bodies = description['bodies'];
  //var constraints = description['constraints'];

  var i;
  var r;
  for (i = 0; i < shapes.length; i++) {
    r = verifyShapeDescription(shapes[i]);
    if (r == false) {
      return false;
    }
  }

  for (i = 0; i < bodies.length; i++) {
    r = verifyBodyDescription(bodies[i], shapes);
    if (r == false) {
      return false;
    }
  }

  return true;
}

function verifyShapeExists(name, shapes) {
  var i;
  for (i = 0; i < shapes.length; i++) {
    if (shapes[i].name == name) {
      return true;
    }
  }
  return false;
}

function verifyBodyDescription(body, shapes) {
  var shapeName = body['shape'];
  var mass = body['mass'];
  var friction = body['friction'];
  var transform = body['transform'];
  if (shapeName == undefined) {
    console.log('Body needs a shapename.');
    return false;
  }
  if (mass == undefined) {
    console.log('Body needs a mass.');
    return false;
  }
  if (friction == undefined) {
    console.log('Body needs a friction.');
    return false;
  }
  if (transform == undefined) {
    console.log('Body needs a transform.');
    return false;
  }
  if (transform[0] == undefined) {
    console.log('Body needs a transform array.');
    return false;
  }
  return verifyShapeExists(shapeName, shapes);
}

function verifyShapeDescription(shape) {
  if (shape['name'] == undefined) {
    console.log('Shape needs a name.');
    return false;
  }

  var type = shape['type'];

  if (type != "cube" &&
      type != "sphere" &&
      type != "cylinder" &&
      type != "convex") {
        console.log('Shape type - ' + type + ' not supported.');
        return false;
      }

  if (type == "cube") {
    return verifyCubeDescription(shape);
  }

  if (type == "sphere") {
    return verifySphereDescription(shape);
  }

  if (type == "cylinder") {
    return verifyCylinderDescription(shape);
  }

  if (type == "convex") {
    return verifyConvexDescription(shape);
  }

  return false;
}

function verifyCubeDescription(shape) {
  if (shape['wx'] == undefined) {
    return false;
  }
  if (shape['wy'] == undefined) {
    return false;
  }
  if (shape['wz'] == undefined) {
    return false;
  }
  return true;
}

function verifySphereDescription(shape) {
  if (shape['radius'] == undefined) {
    return false;
  }
  return true;
}

function verifyCylinderDescription(shape) {
  if (shape['radius'] == undefined) {
    return false;
  }
  if (shape['height'] == undefined) {
    return false;
  }
  return true;
}

function verifyConvexDescription(shape) {
  if (shape['points'] == undefined) {
    return false;
  }
  if (shape['points'][0] == undefined) {
    return false;
  }
  return true;
}
