describe('Date', function () {

    describe('now', function () {
        it('should be the current time', function () {
            expect(Date.now() === new Date().getTime()).toBe(true);
        });
    });

    describe("parse", function () {
        // TODO: Write the rest of the test.

        it('should support extended years', function () {

            expect(Date.parse('0001-01-01T00:00:00Z')).toBe(-62135596800000);
            expect(Date.parse('+275760-09-13T00:00:00.000Z')).toBe(8.64e15);
            expect(Date.parse('+033658-09-27T01:46:40.000Z')).toBe(1e15);
            expect(Date.parse('-000001-01-01T00:00:00Z')).toBe(-62198755200000);
            expect(Date.parse('+002009-12-15T00:00:00Z')).toBe(1260835200000);

        });

    });

    describe("toISOString", function () {
        // TODO: write the rest of the test.

        it('should support extended years', function () {
            expect(new Date(-62198755200000).toISOString().indexOf('-000001-01-01')).toBe(0);
            expect(new Date(8.64e15).toISOString().indexOf('+275760-09-13')).toBe(0);
        });
    });

    describe("toJSON", function () {
        it('should return the isoString when stringified', function () {
            var date = new Date();
            expect(JSON.stringify(date.toISOString())).toBe(JSON.stringify(date));
        })
    });

});