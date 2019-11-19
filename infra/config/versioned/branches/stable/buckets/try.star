load('//versioned/vars/try.star', 'vars')
vars.bucket.set('try-stable')
vars.cq_group.set('cq-stable')
vars.experiment_percentage.set(100)

load('//lib/builders.star', 'defaults')
defaults.pool.set('luci.chromium.try')

load('//versioned/milestones.star', milestone='stable')
exec('//versioned/milestones/%s/buckets/try.star' % milestone)
