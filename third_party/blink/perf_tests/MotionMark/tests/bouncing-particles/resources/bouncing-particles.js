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
function BouncingParticle(stage)
{
    this._stageSize = stage.size;
    this.size = stage.particleSize;

    this.position = Stage.randomPosition(stage.size.subtract(stage.particleSize));
    this._angle = Stage.randomAngle();
    this._velocity = Stage.randomVelocity(stage.maxVelocity);
    this.rotater = Stage.randomRotater();
}

BouncingParticle.prototype =
{
    get center()
    {
        return this.position.add(this.size.center);
    },

    animate: function(timeDelta)
    {
        this.position = this.position.move(this._angle, this._velocity, timeDelta);
        this.rotater.next(timeDelta);

        // If particle is going to move off right side
        if (this.position.x + this.size.x > this._stageSize.x) {
            // If direction is East-South, go West-South.
            if (this._angle >= 0 && this._angle < Math.PI / 2)
                this._angle = Math.PI - this._angle;
            // If angle is East-North, go West-North.
            else if (this._angle > Math.PI / 2 * 3)
                this._angle = this._angle - (this._angle - Math.PI / 2 * 3) * 2;
            // Make sure the particle does not go outside the stage boundaries.
            this.position.x = this._stageSize.x - this.size.x;
        }

        // If particle is going to move off left side
        if (this.position.x < 0) {
            // If angle is West-South, go East-South.
            if (this._angle > Math.PI / 2 && this._angle < Math.PI)
                this._angle = Math.PI - this._angle;
            // If angle is West-North, go East-North.
            else if (this._angle > Math.PI && this._angle < Math.PI / 2 * 3)
                this._angle = this._angle + (Math.PI / 2 * 3 - this._angle) * 2;
            // Make sure the particle does not go outside the stage boundaries.
            this.position.x = 0;
        }

        // If particle is going to move off bottom side
        if (this.position.y + this.size.y > this._stageSize.y) {
            // If direction is South, go North.
            if (this._angle > 0 && this._angle < Math.PI)
                this._angle = Math.PI * 2 - this._angle;
            // Make sure the particle does not go outside the stage boundaries.
            this.position.y = this._stageSize.y - this.size.y;
        }

        // If particle is going to move off top side
        if (this.position.y < 0) {
            // If direction is North, go South.
            if (this._angle > Math.PI && this._angle < Math.PI * 2)
                this._angle = this._angle - (this._angle - Math.PI) * 2;
            // Make sure the particle does not go outside the stage boundaries.
            this.position.y = 0;
        }
    }
}

BouncingParticlesStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
        this.particles = [];
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);
        this.particleSize = new Point(parseInt(options["particleWidth"]) || 10, parseInt(options["particleHeight"]) || 10);
        this.maxVelocity = Math.max(parseInt(options["maxVelocity"]) || 500, 100);
    },

    parseShapeParameters: function(options)
    {
        this.shape = options["shape"] || "circle";
        this.fill = options["fill"] || "solid";
        this.clip = options["clip"] || "";
        this.blend = options["blend"] || false;
        this.filter = options["filter"] || false;
    },

    animate: function(timeDelta)
    {
        this.particles.forEach(function(particle) {
            particle.animate(timeDelta);
        });
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count > 0) {
            for (var i = 0; i < count; ++i)
                this.particles.push(this.createParticle());
            return;
        }

        count = Math.min(-count, this.particles.length);

        if (typeof(this.particleWillBeRemoved) == "function") {
            for (var i = 0; i < count; ++i)
                this.particleWillBeRemoved(this.particles[this.particles.length - 1 - i]);
        }

        this.particles.splice(-count, count);
    },

    complexity: function()
    {
        return this.particles.length;
    }
});
