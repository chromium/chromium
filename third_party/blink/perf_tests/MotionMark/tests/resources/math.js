/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
SimpleKalmanEstimator = Utilities.createSubclass(Experiment,
    function(processError, measurementError) {
        Experiment.call(this, false);
        var error = .5 * (Math.sqrt(processError * processError + 4 * processError * measurementError) - processError);
        this._gain = error / (error + measurementError);
    }, {

    sample: function(newMeasurement)
    {
        if (!this._initialized) {
            this._initialized = true;
            this.estimate = newMeasurement;
            return;
        }

        this.estimate = this.estimate + this._gain * (newMeasurement - this.estimate);
    },

    reset: function()
    {
        Experiment.prototype.reset.call(this);
        this._initialized = false;
        this.estimate = 0;
    }
});

PIDController = Utilities.createClass(
    function(ysp)
    {
        this._ysp = ysp;
        this._out = 0;

        this._Kp = 0;
        this._stage = PIDController.stages.WARMING;

        this._eold = 0;
        this._I = 0;
    }, {

    // Determines whether the current y is
    //  before ysp => (below ysp if ysp > y0) || (above ysp if ysp < y0)
    //  after ysp => (above ysp if ysp > y0) || (below ysp if ysp < y0)
    _yPosition: function(y)
    {
        return (y < this._ysp) == (this._y0 < this._ysp)
            ? PIDController.yPositions.BEFORE_SETPOINT
            : PIDController.yPositions.AFTER_SETPOINT;
    },

    // Calculate the ultimate distance from y0 after time t. We want to move very
    // slowly at the beginning to see how adding few items to the test can affect
    // its output. The complexity of a single item might be big enough to keep the
    // proportional gain very small but achieves the desired progress. But if y does
    // not change significantly after adding few items, that means we need a much
    // bigger gain. So we need to move over a cubic curve which increases very
    // slowly with small t values but moves very fast with larger t values.
    // The basic formula is: y = t^3
    // Change the formula to reach y=1 after 1000 ms: y = (t/1000)^3
    // Change the formula to reach y=(ysp - y0) after 1000 ms: y = (ysp - y0) * (t/1000)^3
    _distanceUltimate: function(t)
    {
        return (this._ysp - this._y0) * Math.pow(t / 1000, 3);
    },

    // Calculates the distance of y relative to y0. It also ensures we do not return
    // zero by returning a epsilon value in the same direction as ultimate distance.
    _distance: function(y, du)
    {
        const epsilon = 0.0001;
        var d  = y - this._y0;
        return du < 0 ? Math.min(d, -epsilon) : Math.max(d, epsilon);
    },

    // Decides how much the proportional gain should be increased during the manual
    // gain stage. We choose to use the ratio of the ultimate distance to the current
    // distance as an indication of how much the system is responsive. We want
    // to keep the increment under control so it does not cause the system instability
    // So we choose to take the natural logarithm of this ratio.
    _gainIncrement: function(t, y, e)
    {
        var du = this._distanceUltimate(t);
        var d = this._distance(y, du);
        return Math.log(du / d) * 0.1;
    },

    // Update the stage of the controller based on its current stage and the system output
    _updateStage: function(y)
    {
        var yPosition = this._yPosition(y);

        switch (this._stage) {
        case PIDController.stages.WARMING:
            if (yPosition == PIDController.yPositions.AFTER_SETPOINT)
                this._stage = PIDController.stages.OVERSHOOT;
            break;

        case PIDController.stages.OVERSHOOT:
            if (yPosition == PIDController.yPositions.BEFORE_SETPOINT)
                this._stage = PIDController.stages.UNDERSHOOT;
            break;

        case PIDController.stages.UNDERSHOOT:
            if (yPosition == PIDController.yPositions.AFTER_SETPOINT)
                this._stage = PIDController.stages.SATURATE;
            break;
        }
    },

    // Manual tuning is used before calculating the PID controller gains.
    _tuneP: function(e)
    {
        // The output is the proportional term only.
        return this._Kp * e;
    },

    // PID tuning function. Kp, Ti and Td were already calculated
    _tunePID: function(h, y, e)
    {
        // Proportional term.
        var P = this._Kp * e;

        // Integral term is the area under the curve starting from the beginning
        // till the current time.
        this._I += (this._Kp / this._Ti) * ((e + this._eold) / 2) * h;

        // Derivative term is the slope of the curve at the current time.
        var D = (this._Kp * this._Td) * (e - this._eold) / h;

        // The output is a PID function.
       return P + this._I + D;
    },

    // Apply different strategies for the tuning based on the stage of the controller.
    _tune: function(t, h, y, e)
    {
        switch (this._stage) {
        case PIDController.stages.WARMING:
            // This is the first stage of the ZieglerâNichols method. It increments
            // the proportional gain till the system output passes the set-point value.
            if (typeof this._y0 == "undefined") {
                // This is the first time a tuning value is required. We want the test
                // to add only one item. So we need to return -1 which forces us to
                // choose the initial value of Kp to be = -1 / e
                this._y0 = y;
                this._Kp = -1 / e;
            } else {
                // Keep incrementing the Kp as long as we have not reached the
                // set-point yet
                this._Kp += this._gainIncrement(t, y, e);
            }

            return this._tuneP(e);

        case PIDController.stages.OVERSHOOT:
            // This is the second stage of the ZieglerâNichols method. It measures the
            // oscillation period.
            if (typeof this._t0 == "undefined") {
                // t is the time of the beginning of the first overshot
                this._t0 = t;
                this._Kp /= 2;
            }

            return this._tuneP(e);

        case PIDController.stages.UNDERSHOOT:
            // This is the end of the ZieglerâNichols method. We need to calculate the
            // integral and derivative periods.
            if (typeof this._Ti == "undefined") {
                // t is the time of the end of the first overshot
                var Tu = t - this._t0;

                // Calculate the system parameters from Kp and Tu assuming
                // a "some overshoot" control type. See:
                // https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method
                this._Ti = Tu / 2;
                this._Td = Tu / 3;
                this._Kp = 0.33 * this._Kp;

                // Calculate the tracking time.
                this._Tt = Math.sqrt(this._Ti * this._Td);
            }

            return this._tunePID(h, y, e);

        case PIDController.stages.SATURATE:
            return this._tunePID(h, y, e);
        }

        return 0;
    },

    // Ensures the system does not fluctuates.
    _saturate: function(v, e)
    {
        var u = v;

        switch (this._stage) {
        case PIDController.stages.OVERSHOOT:
        case PIDController.stages.UNDERSHOOT:
            // Calculate the min-max values of the saturation actuator.
            if (typeof this._min == "undefined")
                this._min = this._max = this._out;
            else {
                this._min = Math.min(this._min, this._out);
                this._max = Math.max(this._max, this._out);
            }
            break;

        case PIDController.stages.SATURATE:
            const limitPercentage = 0.90;
            var min = this._min > 0 ? Math.min(this._min, this._max * limitPercentage) : this._min;
            var max = this._max < 0 ? Math.max(this._max, this._min * limitPercentage) : this._max;
            var out = this._out + u;

            // Clip the controller output to the min-max values
            out = Math.max(Math.min(max, out), min);
            u = out - this._out;

            // Apply the back-calculation and tracking
            if (u != v)
                u += (this._Kp * this._Tt / this._Ti) * e;
            break;
        }

        this._out += u;
        return u;
    },

    // Called from the benchmark to tune its test. It uses Ziegler-Nichols method
    // to calculate the controller parameters. It then returns a PID tuning value.
    tune: function(t, h, y)
    {
        this._updateStage(y);

        // Current error.
        var e = this._ysp - y;
        var v = this._tune(t, h, y, e);

        // Save e for the next call.
        this._eold = e;

        // Apply back-calculation and tracking to avoid integrator windup
        return this._saturate(v, e);
    }
});

Utilities.extendObject(PIDController, {
    // This enum will be used to tell whether the system output (or the controller input)
    // is moving towards the set-point or away from it.
    yPositions: {
        BEFORE_SETPOINT: 0,
        AFTER_SETPOINT: 1
    },

    // The Ziegler-Nichols method for is used tuning the PID controller. The workflow of
    // the tuning is split into four stages. The first two stages determine the values
    // of the PID controller gains. During these two stages we return the proportional
    // term only. The third stage is used to determine the min-max values of the
    // saturation actuator. In the last stage back-calculation and tracking are applied
    // to avoid integrator windup. During the last two stages, we return a PID control
    // value.
    stages: {
        WARMING: 0,         // Increase the value of the Kp until the system output reaches ysp.
        OVERSHOOT: 1,       // Measure the oscillation period and the overshoot value
        UNDERSHOOT: 2,      // Return PID value and measure the undershoot value
        SATURATE: 3         // Return PID value and apply back-calculation and tracking.
    }
});
