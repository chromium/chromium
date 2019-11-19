load('//lib/bucket.star', bucket_var='var')

vars = struct(
    bucket = bucket_var(default = 'try'),
    cq_group = lucicfg.var(default = 'cq'),
    experiment_percentage = lucicfg.var(),
)
