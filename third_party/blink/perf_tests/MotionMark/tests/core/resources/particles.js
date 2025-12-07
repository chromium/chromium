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
function Particle(stage)
{
    this.stage = stage;
    this.rotater = Stage.randomRotater();
    this.reset();
    this.move();
}

Particle.prototype =
{
    sizeMinimum: 40,
    sizeRange: 10,

    reset: function()
    {
        var randSize = Math.round(Math.pow(Pseudo.random(), 4) * this.sizeRange + this.sizeMinimum);
        this.size = new Point(randSize, randSize);
        this.minPosition = this.size.center;
        this.maxPosition = this.stage.size.subtract(this.minPosition);
    },

    animate: function(timeDelta)
    {
        this.rotater.next(timeDelta);

        this.position = this.position.add(this.velocity.multiply(timeDelta));
        this.velocity.y += 0.03;

        // If particle is going to move off right side
        if (this.position.x > this.maxPosition.x) {
            if (this.velocity.x > 0)
                this.velocity.x *= -1;
            this.position.x = this.maxPosition.x;
        } else if (this.position.x < this.minPosition.x) {
            // If particle is going to move off left side
            if (this.velocity.x < 0)
                this.velocity.x *= -1;
            this.position.x = this.minPosition.x;
        }

        // If particle is going to move off bottom side
        if (this.position.y > this.maxPosition.y) {
            // Adjust direction but maintain magnitude
            var magnitude = this.velocity.length();
            this.velocity.x *= 1.5 + .005 * this.size.x;
            this.velocity = this.velocity.normalize().multiply(magnitude);
            if (Math.abs(this.velocity.y) < 0.7)
                this.reset();
            else {
                if (this.velocity.y > 0)
                    this.velocity.y *= -0.999;
                this.position.y = this.maxPosition.y;
            }
        } else if (this.position.y < this.minPosition.y) {
            // If particle is going to move off top side
            var magnitude = this.velocity.length();
            this.velocity.x *= 1.5 + .005 * this.size.x;
            this.velocity = this.velocity.normalize().multiply(magnitude);
            if (this.velocity.y < 0)
                this.velocity.y *= -0.998;
            this.position.y = this.minPosition.y;
        }

        this.move();
    },

    move: function()
    {
    }
}

ParticlesStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
        this.particles = [];
    }, {

    animate: function(timeDelta)
    {
        timeDelta /= 4;
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

        if (typeof(this.willRemoveParticle) == "function") {
            for (var i = 0; i < count; ++i)
                this.willRemoveParticle(this.particles[i]);
        }

        this.particles.splice(0, count);
    },

    complexity: function()
    {
        return this.particles.length;
    }
});
