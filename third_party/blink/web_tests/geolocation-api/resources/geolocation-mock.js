/*
 * geolocation-mock contains a mock implementation of Geolocation and
 * PermissionService.
 */

"use strict";

class GeolocationMock {
  constructor() {
    this.geolocationServiceInterceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.GeolocationService.name);
    this.geolocationServiceInterceptor_.oninterfacerequest =
        e => this.connectGeolocationService_(e.handle);
    this.geolocationServiceInterceptor_.start();

    /**
     * The next geoposition to return in response to a queryNextPosition()
     * call.
    */
    this.geoposition_ = null;

    /**
     * While true, position requests will result in a timeout error.
     */
    this.shouldTimeout_ = false;

    /**
     * A pending request for permission awaiting a decision to be set via a
     * setGeolocationPermission call.
     *
     * @type {?Function}
     */
    this.pendingPermissionRequest_ = null;

    /**
     * The status to respond to permission requests with. If set to ASK, then
     * permission requests will block until setGeolocationPermission is called
     * to allow or deny permission requests.
     *
     * @type {!blink.mojom.PermissionStatus}
     */
    this.permissionStatus_ = blink.mojom.PermissionStatus.ASK;
    this.rejectGeolocationServiceConnections_ = false;

    this.geolocationBindingSet_ = new mojo.BindingSet(
        device.mojom.Geolocation);
    this.geolocationServiceBindingSet_ = new mojo.BindingSet(
        blink.mojom.GeolocationService);
  }

  connectGeolocationService_(handle) {
    if (this.rejectGeolocationServiceConnections_) {
      handle.close();
      return;
    }
    this.geolocationServiceBindingSet_.addBinding(this, handle);
  }

  setHighAccuracy(highAccuracy) {
    // FIXME: We need to add some tests regarding "high accuracy" mode.
    // See https://bugs.webkit.org/show_bug.cgi?id=49438
  }

  /**
   * A mock implementation of GeolocationService.queryNextPosition(). This
   * returns the position set by a call to setGeolocationPosition() or
   * setGeolocationPositionUnavailableError().
   */
  queryNextPosition() {
    if (this.shouldTimeout_) {
      // Return a promise that will never be resolved. Since no geoposition is
      // returned, the request will eventually time out.
      return new Promise((resolve, reject) => {});
    }
    if (!this.geoposition_) {
      this.setGeolocationPositionUnavailableError(
          'Test error: position not set before call to queryNextPosition()');
    }
    let geoposition = this.geoposition_;
    this.geoposition_ = null;
    return Promise.resolve({geoposition});
  }

  /**
   * Sets the position to return to the next queryNextPosition() call. If any
   * queryNextPosition() requests are outstanding, they will all receive the
   * position set by this call.
   */
  setGeolocationPosition(latitude, longitude, accuracy, altitude,
                         altitudeAccuracy, heading, speed) {
    this.geoposition_ = new device.mojom.Geoposition();
    this.geoposition_.latitude = latitude;
    this.geoposition_.longitude = longitude;
    this.geoposition_.accuracy = accuracy;
    this.geoposition_.altitude = altitude;
    this.geoposition_.altitudeAccuracy = altitudeAccuracy;
    this.geoposition_.heading = heading;
    this.geoposition_.speed = speed;
    this.geoposition_.timestamp = new mojoBase.mojom.Time();
    // The new Date().getTime() returns the number of milliseconds since the
    // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
    // device.mojom.Geoposition represents the value of microseconds since the
    // Windows FILETIME epoch (1601-01-01 00:00:00 UTC). So add the delta when
    // sets the |internalValue|. See more info in //base/time/time.h.
    const windowsEpoch = Date.UTC(1601,0,1,0,0,0,0);
    const unixEpoch = Date.UTC(1970,0,1,0,0,0,0);
    // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
    const epochDeltaInMs = unixEpoch - windowsEpoch;

    this.geoposition_.timestamp.internalValue =
        (new Date().getTime() + epochDeltaInMs)  * 1000;
    this.geoposition_.errorMessage = '';
    this.geoposition_.valid = true;
  }

  /**
   * Sets the error message to return to the next queryNextPosition() call. If
   * any queryNextPosition() requests are outstanding, they will all receive
   * the error set by this call.
   */
  setGeolocationPositionUnavailableError(message) {
    this.geoposition_ = new device.mojom.Geoposition();
    this.geoposition_.valid = false;
    this.geoposition_.timestamp = new mojoBase.mojom.Time();
    this.geoposition_.errorMessage = message;
    this.geoposition_.errorCode =
        device.mojom.Geoposition.ErrorCode.POSITION_UNAVAILABLE;
  }

  /**
   * Sets whether geolocation requests should cause timeout errors.
   */
  setGeolocationTimeoutError(shouldTimeout) {
    this.shouldTimeout_ = shouldTimeout;
  }

  /**
   * Reject any connection requests for the geolocation service. This will
   * trigger a connection error in the client.
   */
  rejectGeolocationServiceConnections() {
    this.rejectGeolocationServiceConnections_ = true;
  }

  /**
   * A mock implementation of GeolocationService.createGeolocation().
   * This accepts the request as long as the permission has been set to
   * granted.
   */
  createGeolocation(request, user_gesture) {
    switch (this.permissionStatus_) {
     case blink.mojom.PermissionStatus.ASK:
      return new Promise((resolve, reject) => {
        setTimeout(() => {
          resolve(this.createGeolocation(request, user_gesture));
        }, 50);
      });
      setTimeout(() => { this.createGeolocation(request, user_gesture)}, 50);
      break;

     case blink.mojom.PermissionStatus.GRANTED:
      this.geolocationBindingSet_.addBinding(this, request);
      break;

     default:
      request.close();
    }
    return Promise.resolve(this.permissionStatus_);
  }

  /**
   * Sets whether the next geolocation permission request should be allowed.
   */
  setGeolocationPermission(allowed) {
    this.permissionStatus_ = allowed ? blink.mojom.PermissionStatus.GRANTED
                                     : blink.mojom.PermissionStatus.DENIED;
  }
}

let geolocationMock = new GeolocationMock();
