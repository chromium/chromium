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
BouncingCanvasParticle = Utilities.createSubclass(BouncingParticle,
    function(stage, shape)
    {
        BouncingParticle.call(this, stage);
        this.context = stage.context;
        this._shape = shape;
        this._clip = stage.clip;
    }, {

    applyRotation: function()
    {
        if (this._shape == "circle")
            return;

        this.context.translate(this.size.x / 2, this.size.y / 2);
        this.context.rotate(this.rotater.degree() * Math.PI / 180);
        this.context.translate(-this.size.x / 2, -this.size.y / 2);
    },

    applyClipping: function()
    {
        var clipPoints = BouncingCanvasParticle.clips[this._clip];
        if (!clipPoints)
            return;

        this.context.beginPath();
        clipPoints.forEach(function(point, index) {
            var point = this.size.multiply(point);
            if (!index)
                this.context.moveTo(point.x, point.y);
            else
                this.context.lineTo(point.x, point.y);
        }, this);

        this.context.closePath();
        this.context.clip();
    },

    _draw: function()
    {
        throw "Not implemented";
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this.context.save();
            this.context.translate(this.position.x, this.position.y);
            this._draw();
        this.context.restore();
    }
});

BouncingCanvasParticle.clips = {
    star: [
        new Point(0.50, 0.00),
        new Point(0.38, 0.38),
        new Point(0.00, 0.38),
        new Point(0.30, 0.60),
        new Point(0.18, 1.00),
        new Point(0.50, 0.75),
        new Point(0.82, 1.00),
        new Point(0.70, 0.60),
        new Point(1.00, 0.38),
        new Point(0.62, 0.38)
    ]
};

BouncingCanvasParticlesStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.context = this.element.getContext("2d");
    },

    animate: function(timeDelta)
    {
        this.context.clearRect(0, 0, this.size.x, this.size.y);
        this.particles.forEach(function(particle) {
            particle.animate(timeDelta);
        });
    }
});
