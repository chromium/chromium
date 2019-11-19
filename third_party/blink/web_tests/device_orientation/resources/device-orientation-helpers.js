'use strict';

const MOTION_ROTATION_EPSILON = 1e-8;

function assertTestRunner() {
  assert_true(window.testRunner instanceof Object,
    "This test can not be run without the window.testRunner.");
}

function generateMotionData(accelerationX, accelerationY, accelerationZ,
                            accelerationIncludingGravityX,
                            accelerationIncludingGravityY,
                            accelerationIncludingGravityZ,
                            rotationRateAlpha, rotationRateBeta, rotationRateGamma,
                            interval = 16) {
  var motionData = {accelerationX: accelerationX,
                    accelerationY: accelerationY,
                    accelerationZ: accelerationZ,
                    accelerationIncludingGravityX: accelerationIncludingGravityX,
                    accelerationIncludingGravityY: accelerationIncludingGravityY,
                    accelerationIncludingGravityZ: accelerationIncludingGravityZ,
                    rotationRateAlpha: rotationRateAlpha,
                    rotationRateBeta: rotationRateBeta,
                    rotationRateGamma: rotationRateGamma,
                    interval: interval};
  return motionData;
}

function generateOrientationData(alpha, beta, gamma, absolute) {
  var orientationData = {alpha: alpha,
                         beta: beta,
                         gamma: gamma,
                         absolute: absolute};
  return orientationData;
}

// Device[Orientation|Motion]EventPump treat NaN as a missing value.
let nullToNan = x => (x === null ? NaN : x);

function setMockMotionData(sensorProvider, motionData) {
  const degToRad = Math.PI / 180;
  return Promise.all([
      setMockSensorDataForType(sensorProvider, "Accelerometer", [
          nullToNan(motionData.accelerationIncludingGravityX),
          nullToNan(motionData.accelerationIncludingGravityY),
          nullToNan(motionData.accelerationIncludingGravityZ),
      ]),
      setMockSensorDataForType(sensorProvider, "LinearAccelerationSensor", [
          nullToNan(motionData.accelerationX),
          nullToNan(motionData.accelerationY),
          nullToNan(motionData.accelerationZ),
      ]),
      setMockSensorDataForType(sensorProvider, "Gyroscope", [
          nullToNan(motionData.rotationRateAlpha) * degToRad,
          nullToNan(motionData.rotationRateBeta) * degToRad,
          nullToNan(motionData.rotationRateGamma) * degToRad,
      ]),
  ]);
}

function setMockOrientationData(sensorProvider, orientationData) {
  let sensorType = orientationData.absolute
      ? "AbsoluteOrientationEulerAngles" : "RelativeOrientationEulerAngles";
  return setMockSensorDataForType(sensorProvider, sensorType, [
      nullToNan(orientationData.beta),
      nullToNan(orientationData.gamma),
      nullToNan(orientationData.alpha),
  ]);
}

function checkMotion(event, expectedMotionData) {
  assert_equals(event.acceleration.x, expectedMotionData.accelerationX, "acceleration.x");
  assert_equals(event.acceleration.y, expectedMotionData.accelerationY, "acceleration.y");
  assert_equals(event.acceleration.z, expectedMotionData.accelerationZ, "acceleration.z");

  assert_equals(event.accelerationIncludingGravity.x, expectedMotionData.accelerationIncludingGravityX, "accelerationIncludingGravity.x");
  assert_equals(event.accelerationIncludingGravity.y, expectedMotionData.accelerationIncludingGravityY, "accelerationIncludingGravity.y");
  assert_equals(event.accelerationIncludingGravity.z, expectedMotionData.accelerationIncludingGravityZ, "accelerationIncludingGravity.z");

  assert_approx_equals(event.rotationRate.alpha, expectedMotionData.rotationRateAlpha, MOTION_ROTATION_EPSILON, "rotationRate.alpha");
  assert_approx_equals(event.rotationRate.beta, expectedMotionData.rotationRateBeta, MOTION_ROTATION_EPSILON, "rotationRate.beta");
  assert_approx_equals(event.rotationRate.gamma, expectedMotionData.rotationRateGamma, MOTION_ROTATION_EPSILON, "rotationRate.gamma");

  assert_equals(event.interval, expectedMotionData.interval, "interval");
}

function checkOrientation(event, expectedOrientationData) {
  assert_equals(event.alpha, expectedOrientationData.alpha, "alpha");
  assert_equals(event.beta, expectedOrientationData.beta, "beta");
  assert_equals(event.gamma, expectedOrientationData.gamma, "gamma");

  assert_equals(event.absolute, expectedOrientationData.absolute, "absolute");
}

function waitForOrientation(expectedOrientationData, targetWindow = window) {
  return waitForEvent(
      new DeviceOrientationEvent('deviceorientation', {
        alpha: expectedOrientationData.alpha,
        beta: expectedOrientationData.beta,
        gamma: expectedOrientationData.gamma,
        absolute: expectedOrientationData.absolute,
      }),
      targetWindow);
}

function waitForAbsoluteOrientation(expectedOrientationData, targetWindow = window) {
  return waitForEvent(
      new DeviceOrientationEvent('deviceorientationabsolute', {
        alpha: expectedOrientationData.alpha,
        beta: expectedOrientationData.beta,
        gamma: expectedOrientationData.gamma,
        absolute: expectedOrientationData.absolute,
      }),
      targetWindow);
}

function waitForMotion(expectedMotionData, targetWindow = window) {
  return waitForEvent(
      new DeviceMotionEvent('devicemotion', {
        acceleration: {
          x: expectedMotionData.accelerationX,
          y: expectedMotionData.accelerationY,
          z: expectedMotionData.accelerationZ,
        },
        accelerationIncludingGravity: {
          x: expectedMotionData.accelerationIncludingGravityX,
          y: expectedMotionData.accelerationIncludingGravityY,
          z: expectedMotionData.accelerationIncludingGravityZ,
        },
        rotationRate: {
          alpha: expectedMotionData.rotationRateAlpha,
          beta: expectedMotionData.rotationRateBeta,
          gamma: expectedMotionData.rotationRateGamma,
        },
        interval: expectedMotionData.interval,
      }),
      targetWindow);
}
